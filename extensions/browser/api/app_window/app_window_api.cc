// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/app_window/app_window_api.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_client.h"
#include "extensions/browser/app_window/app_window_contents.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/api/app_window.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/image_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace app_window = extensions::api::app_window;
namespace Create = app_window::Create;

namespace extensions {

namespace app_window_constants {
constexpr char kInvalidWindowId[] =
    "The window id can not be more than 256 characters long.";
constexpr char kInvalidColorSpecification[] =
    "The color specification could not be parsed.";
constexpr char kColorWithFrameNone[] =
    "Windows with no frame cannot have a color.";
constexpr char kInactiveColorWithoutColor[] =
    "frame.inactiveColor must be used with frame.color.";
constexpr char kConflictingBoundsOptions[] =
    "The $1 property cannot be specified for both inner and outer bounds.";
constexpr char kAlwaysOnTopPermission[] =
    "The \"app.window.alwaysOnTop\" permission is required.";
constexpr char kInvalidUrlParameter[] =
    "The URL used for window creation must be local for security reasons.";
constexpr char kAlphaEnabledWrongChannel[] =
    "The alphaEnabled option requires dev channel or newer.";
constexpr char kAlphaEnabledMissingPermission[] =
    "The alphaEnabled option requires app.window.alpha permission.";
constexpr char kAlphaEnabledNeedsFrameNone[] =
    "The alphaEnabled option can only be used with \"frame: 'none'\".";
constexpr char kImeWindowMissingPermission[] =
    "Extensions require the \"app.window.ime\" permission to create windows.";
constexpr char kImeOptionIsNotSupported[] =
    "The \"ime\" option is not supported for platform app.";
#if !defined(OS_CHROMEOS)
constexpr char kImeWindowUnsupportedPlatform[] =
    "The \"ime\" option can only be used on ChromeOS.";
#else
constexpr char kImeWindowMustBeImeWindow[] =
    "IME extensions must create an IME window ( with \"ime: true\" and "
    "\"frame: 'none'\"). Panels are no longer supported for IME extensions.";
#endif
constexpr char kShowInShelfWindowKeyNotSet[] =
    "The \"showInShelf\" option requires the \"id\" option to be set.";
constexpr char kLockScreenActionRequiresLockScreenContext[] =
    "The lockScreenAction option requires lock screen app context.";
constexpr char kLockScreenActionRequiresLockScreenPermission[] =
    "The lockScreenAction option requires lockScreen permission.";
constexpr char kAppWindowCreationFailed[] = "Failed to create the app window.";
constexpr char kPrematureWindowClose[] =
    "App window is closed before ready to commit first navigation.";
}  // namespace app_window_constants

const char kNoneFrameOption[] = "none";

namespace {

// If the same property is specified for the inner and outer bounds, raise an
// error.
bool CheckBoundsConflict(const std::unique_ptr<int>& inner_property,
                         const std::unique_ptr<int>& outer_property,
                         const std::string& property_name,
                         std::string* error) {
  if (inner_property.get() && outer_property.get()) {
    std::vector<std::string> subst;
    subst.push_back(property_name);
    *error = base::ReplaceStringPlaceholders(
        app_window_constants::kConflictingBoundsOptions, subst, NULL);
    return false;
  }

  return true;
}

// Copy over the bounds specification properties from the API to the
// AppWindow::CreateParams.
void CopyBoundsSpec(const app_window::BoundsSpecification* input_spec,
                    AppWindow::BoundsSpecification* create_spec) {
  if (!input_spec)
    return;

  if (input_spec->left.get())
    create_spec->bounds.set_x(*input_spec->left);
  if (input_spec->top.get())
    create_spec->bounds.set_y(*input_spec->top);
  if (input_spec->width.get())
    create_spec->bounds.set_width(*input_spec->width);
  if (input_spec->height.get())
    create_spec->bounds.set_height(*input_spec->height);
  if (input_spec->min_width.get())
    create_spec->minimum_size.set_width(*input_spec->min_width);
  if (input_spec->min_height.get())
    create_spec->minimum_size.set_height(*input_spec->min_height);
  if (input_spec->max_width.get())
    create_spec->maximum_size.set_width(*input_spec->max_width);
  if (input_spec->max_height.get())
    create_spec->maximum_size.set_height(*input_spec->max_height);
}

}  // namespace

AppWindowCreateFunction::AppWindowCreateFunction() {}

ExtensionFunction::ResponseAction AppWindowCreateFunction::Run() {
  // Don't create app window if the system is shutting down.
  if (ExtensionsBrowserClient::Get()->IsShuttingDown())
    return RespondNow(Error(kUnknownErrorDoNotUse));

  std::unique_ptr<Create::Params> params(Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url = extension()->GetResourceURL(params->url);
  // Allow absolute URLs for component apps, otherwise prepend the extension
  // path.
  // TODO(devlin): Investigate if this is still used. If not, kill it dead!
  GURL absolute = GURL(params->url);
  if (absolute.has_scheme()) {
    if (extension()->location() == Manifest::COMPONENT) {
      url = absolute;
    } else {
      // Show error when url passed isn't local.
      return RespondNow(Error(app_window_constants::kInvalidUrlParameter));
    }
  }

  // TODO(jeremya): figure out a way to pass the opening WebContents through to
  // AppWindow::Create so we can set the opener at create time rather than
  // with a hack in AppWindowCustomBindings::GetView().
  AppWindow::CreateParams create_params;
  app_window::CreateWindowOptions* options = params->options.get();
  if (options) {
    if (options->id.get()) {
      // TODO(mek): use URL if no id specified?
      // Limit length of id to 256 characters.
      if (options->id->length() > 256)
        return RespondNow(Error(app_window_constants::kInvalidWindowId));

      create_params.window_key = *options->id;

      if (options->singleton && *options->singleton == false) {
        WriteToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                       "The 'singleton' option in chrome.apps.window.create() "
                       "is deprecated!"
                       " Change your code to no longer rely on this.");
      }

      if (!options->singleton || *options->singleton) {
        AppWindow* existing_window =
            AppWindowRegistry::Get(browser_context())
                ->GetAppWindowForAppAndKey(extension_id(),
                                           create_params.window_key);
        if (existing_window) {
          content::RenderFrameHost* existing_frame =
              existing_window->web_contents()->GetMainFrame();
          int frame_id = MSG_ROUTING_NONE;
          if (source_process_id() == existing_frame->GetProcess()->GetID()) {
            frame_id = existing_frame->GetRoutingID();
          }

          if (!options->hidden.get() || !*options->hidden) {
            if (options->focused.get() && !*options->focused)
              existing_window->Show(AppWindow::SHOW_INACTIVE);
            else
              existing_window->Show(AppWindow::SHOW_ACTIVE);
          }

          std::unique_ptr<base::DictionaryValue> result(
              new base::DictionaryValue);
          result->SetInteger("frameId", frame_id);
          existing_window->GetSerializedState(result.get());
          result->SetBoolean("existingWindow", true);
          // We should not return the window until that window is properly
          // initialized. Hence, adding a callback for window first navigation
          // completion.
          if (existing_window->DidFinishFirstNavigation()) 
            return RespondNow(OneArgument(std::move(result)));
          
          existing_window->AddOnDidFinishFirstNavigationCallback(
            base::BindOnce(&AppWindowCreateFunction::
                           OnAppWindowFinishedFirstNavigationOrClosed,
                           this, OneArgument(std::move(result))));
          return RespondLater();
        }
      }
    }

    std::string error;
    if (!GetBoundsSpec(*options, &create_params, &error))
      return RespondNow(Error(error));

    if (options->type == app_window::WINDOW_TYPE_PANEL) {
      WriteToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                     "Panels are no longer supported.");
    }

    if (!GetFrameOptions(*options, &create_params, &error))
      return RespondNow(Error(error));

    if (extension()->GetType() == Manifest::TYPE_EXTENSION) {
      // Whitelisted IME extensions are allowed to use this API to create IME
      // specific windows to show accented characters or suggestions.
      if (!extension()->permissions_data()->HasAPIPermission(
              APIPermission::kImeWindowEnabled)) {
        return RespondNow(
            Error(app_window_constants::kImeWindowMissingPermission));
      }

#if !defined(OS_CHROMEOS)
      // IME window is only supported on ChromeOS.
      return RespondNow(
          Error(app_window_constants::kImeWindowUnsupportedPlatform));
#else
      // IME extensions must create ime window (with "ime: true" and
      // "frame: none").
      if (options->ime.get() && *options->ime.get() &&
          create_params.frame == AppWindow::FRAME_NONE) {
        create_params.is_ime_window = true;
      } else {
        return RespondNow(
            Error(app_window_constants::kImeWindowMustBeImeWindow));
      }
#endif  // OS_CHROMEOS
    } else {
      if (options->ime.get()) {
        return RespondNow(
            Error(app_window_constants::kImeOptionIsNotSupported));
      }
    }

    if (options->alpha_enabled.get()) {
      const char* const kWhitelist[] = {
#if defined(OS_CHROMEOS)
        "B58B99751225318C7EB8CF4688B5434661083E07",  // http://crbug.com/410550
        "06BE211D5F014BAB34BC22D9DDA09C63A81D828E",  // http://crbug.com/425539
        "F94EE6AB36D6C6588670B2B01EB65212D9C64E33",
        "B9EF10DDFEA11EF77873CC5009809E5037FC4C7A",  // http://crbug.com/435380
#endif
        "0F42756099D914A026DADFA182871C015735DD95",  // http://crbug.com/323773
        "2D22CDB6583FD0A13758AEBE8B15E45208B4E9A7",
        "E7E2461CE072DF036CF9592740196159E2D7C089",  // http://crbug.com/356200
        "A74A4D44C7CFCD8844830E6140C8D763E12DD8F3",
        "312745D9BF916161191143F6490085EEA0434997",
        "53041A2FA309EECED01FFC751E7399186E860B2C",
        "A07A5B743CD82A1C2579DB77D353C98A23201EEF",  // http://crbug.com/413748
        "F16F23C83C5F6DAD9B65A120448B34056DD80691",
        "0F585FB1D0FDFBEBCE1FEB5E9DFFB6DA476B8C9B"
      };
      if (AppWindowClient::Get()->IsCurrentChannelOlderThanDev() &&
          !SimpleFeature::IsIdInArray(extension_id(), kWhitelist,
                                      base::size(kWhitelist))) {
        return RespondNow(
            Error(app_window_constants::kAlphaEnabledWrongChannel));
      }
      if (!extension()->permissions_data()->HasAPIPermission(
              APIPermission::kAlphaEnabled)) {
        return RespondNow(
            Error(app_window_constants::kAlphaEnabledMissingPermission));
      }
      if (create_params.frame != AppWindow::FRAME_NONE) {
        return RespondNow(
            Error(app_window_constants::kAlphaEnabledNeedsFrameNone));
      }
#if defined(USE_AURA)
      create_params.alpha_enabled = *options->alpha_enabled;
#else
      // Transparency is only supported on Aura.
      // Fallback to creating an opaque window (by ignoring alphaEnabled).
#endif
    }

    if (options->hidden.get())
      create_params.hidden = *options->hidden;

    if (options->resizable.get())
      create_params.resizable = *options->resizable;

    if (options->always_on_top.get()) {
      create_params.always_on_top = *options->always_on_top;

      if (create_params.always_on_top &&
          !extension()->permissions_data()->HasAPIPermission(
              APIPermission::kAlwaysOnTopWindows)) {
        return RespondNow(Error(app_window_constants::kAlwaysOnTopPermission));
      }
    }

    if (options->focused.get())
      create_params.focused = *options->focused;

    if (options->visible_on_all_workspaces.get()) {
      create_params.visible_on_all_workspaces =
          *options->visible_on_all_workspaces;
    }

    if (options->show_in_shelf.get()) {
      create_params.show_in_shelf = *options->show_in_shelf.get();

      if (create_params.show_in_shelf && create_params.window_key.empty()) {
        return RespondNow(
            Error(app_window_constants::kShowInShelfWindowKeyNotSet));
      }
    }

    if (options->icon.get()) {
      // First, check if the window icon URL is a valid global URL.
      create_params.window_icon_url = GURL(*options->icon.get());

      // If the URL is not global, check for a valid extension local URL.
      if (!create_params.window_icon_url.is_valid()) {
        create_params.window_icon_url =
            extension()->GetResourceURL(*options->icon.get());
      }
    }

    switch (options->state) {
      case app_window::STATE_NONE:
      case app_window::STATE_NORMAL:
        break;
      case app_window::STATE_FULLSCREEN:
        create_params.state = ui::SHOW_STATE_FULLSCREEN;
        break;
      case app_window::STATE_MAXIMIZED:
        create_params.state = ui::SHOW_STATE_MAXIMIZED;
        break;
      case app_window::STATE_MINIMIZED:
        create_params.state = ui::SHOW_STATE_MINIMIZED;
        break;
    }
  }

  api::app_runtime::ActionType action_type = api::app_runtime::ACTION_TYPE_NONE;
  if (options &&
      options->lock_screen_action != api::app_runtime::ACTION_TYPE_NONE) {
    if (source_context_type() != Feature::LOCK_SCREEN_EXTENSION_CONTEXT) {
      return RespondNow(Error(
          app_window_constants::kLockScreenActionRequiresLockScreenContext));
    }

    if (!extension()->permissions_data()->HasAPIPermission(
            APIPermission::kLockScreen)) {
      return RespondNow(Error(
          app_window_constants::kLockScreenActionRequiresLockScreenPermission));
    }

    action_type = options->lock_screen_action;
    create_params.show_on_lock_screen = true;
  }

  create_params.creator_process_id = source_process_id();

  AppWindow* app_window = nullptr;
  if (action_type == api::app_runtime::ACTION_TYPE_NONE) {
    app_window =
        AppWindowClient::Get()->CreateAppWindow(browser_context(), extension());
  } else {
    app_window = AppWindowClient::Get()->CreateAppWindowForLockScreenAction(
        browser_context(), extension(), action_type);
  }

  // App window client might refuse to create an app window, e.g. when the app
  // attempts to create a lock screen action handler window when the action was
  // not requested.
  if (!app_window)
    return RespondNow(Error(app_window_constants::kAppWindowCreationFailed));

  app_window->Init(url, new AppWindowContentsImpl(app_window),
                   render_frame_host(), create_params);

  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode() &&
      !app_window->is_ime_window()) {
    app_window->ForcedFullscreen();
  }

  content::RenderFrameHost* created_frame =
      app_window->web_contents()->GetMainFrame();
  int frame_id = MSG_ROUTING_NONE;
  if (create_params.creator_process_id == created_frame->GetProcess()->GetID())
    frame_id = created_frame->GetRoutingID();

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  result->SetInteger("frameId", frame_id);
  result->SetString("id", app_window->window_key());
  app_window->GetSerializedState(result.get());
  ResponseValue result_arg = OneArgument(std::move(result));

  if (AppWindowRegistry::Get(browser_context())
          ->HadDevToolsAttached(app_window->web_contents())) {
    AppWindowClient::Get()->OpenDevToolsWindow(
        app_window->web_contents(),
        base::Bind(&AppWindowCreateFunction::Respond, this,
                   base::Passed(&result_arg)));
    // OpenDevToolsWindow might have already responded.
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  // Delay sending the response until the newly created window has finished its
  // navigation or was closed during that process.
  // AddOnDidFinishFirstNavigationCallback() will respond asynchrously.
  app_window->AddOnDidFinishFirstNavigationCallback(base::BindOnce(
      &AppWindowCreateFunction::OnAppWindowFinishedFirstNavigationOrClosed,
      this, std::move(result_arg)));
  return RespondLater();
}

void AppWindowCreateFunction::OnAppWindowFinishedFirstNavigationOrClosed(
    ResponseValue result_arg,
    bool did_finish) {
  DCHECK(!did_respond());

  if (!did_finish) {
    Respond(Error(app_window_constants::kPrematureWindowClose));
    return;
  }

  Respond(std::move(result_arg));
}

bool AppWindowCreateFunction::GetBoundsSpec(
    const app_window::CreateWindowOptions& options,
    AppWindow::CreateParams* params,
    std::string* error) {
  DCHECK(params);
  DCHECK(error);

  if (options.inner_bounds.get() || options.outer_bounds.get()) {
    // Parse the inner and outer bounds specifications. If developers use the
    // new API, the deprecated fields will be ignored - do not attempt to merge
    // them.

    const app_window::BoundsSpecification* inner_bounds =
        options.inner_bounds.get();
    const app_window::BoundsSpecification* outer_bounds =
        options.outer_bounds.get();
    if (inner_bounds && outer_bounds) {
      if (!CheckBoundsConflict(
               inner_bounds->left, outer_bounds->left, "left", error)) {
        return false;
      }
      if (!CheckBoundsConflict(
               inner_bounds->top, outer_bounds->top, "top", error)) {
        return false;
      }
      if (!CheckBoundsConflict(
               inner_bounds->width, outer_bounds->width, "width", error)) {
        return false;
      }
      if (!CheckBoundsConflict(
               inner_bounds->height, outer_bounds->height, "height", error)) {
        return false;
      }
      if (!CheckBoundsConflict(inner_bounds->min_width,
                               outer_bounds->min_width,
                               "minWidth",
                               error)) {
        return false;
      }
      if (!CheckBoundsConflict(inner_bounds->min_height,
                               outer_bounds->min_height,
                               "minHeight",
                               error)) {
        return false;
      }
      if (!CheckBoundsConflict(inner_bounds->max_width,
                               outer_bounds->max_width,
                               "maxWidth",
                               error)) {
        return false;
      }
      if (!CheckBoundsConflict(inner_bounds->max_height,
                               outer_bounds->max_height,
                               "maxHeight",
                               error)) {
        return false;
      }
    }

    CopyBoundsSpec(inner_bounds, &(params->content_spec));
    CopyBoundsSpec(outer_bounds, &(params->window_spec));
  } else {
    // Parse deprecated fields.
    // Due to a bug in NativeAppWindow::GetFrameInsets() on Windows and ChromeOS
    // the bounds set the position of the window and the size of the content.
    // This will be preserved as apps may be relying on this behavior.

    if (options.default_width.get())
      params->content_spec.bounds.set_width(*options.default_width);
    if (options.default_height.get())
      params->content_spec.bounds.set_height(*options.default_height);
    if (options.default_left.get())
      params->window_spec.bounds.set_x(*options.default_left);
    if (options.default_top.get())
      params->window_spec.bounds.set_y(*options.default_top);

    if (options.width.get())
      params->content_spec.bounds.set_width(*options.width);
    if (options.height.get())
      params->content_spec.bounds.set_height(*options.height);
    if (options.left.get())
      params->window_spec.bounds.set_x(*options.left);
    if (options.top.get())
      params->window_spec.bounds.set_y(*options.top);

    if (options.bounds.get()) {
      app_window::ContentBounds* bounds = options.bounds.get();
      if (bounds->width.get())
        params->content_spec.bounds.set_width(*bounds->width);
      if (bounds->height.get())
        params->content_spec.bounds.set_height(*bounds->height);
      if (bounds->left.get())
        params->window_spec.bounds.set_x(*bounds->left);
      if (bounds->top.get())
        params->window_spec.bounds.set_y(*bounds->top);
    }

    gfx::Size& minimum_size = params->content_spec.minimum_size;
    if (options.min_width.get())
      minimum_size.set_width(*options.min_width);
    if (options.min_height.get())
      minimum_size.set_height(*options.min_height);
    gfx::Size& maximum_size = params->content_spec.maximum_size;
    if (options.max_width.get())
      maximum_size.set_width(*options.max_width);
    if (options.max_height.get())
      maximum_size.set_height(*options.max_height);
  }

  return true;
}

AppWindow::Frame AppWindowCreateFunction::GetFrameFromString(
    const std::string& frame_string) {
  if (frame_string == kNoneFrameOption)
    return AppWindow::FRAME_NONE;

  return AppWindow::FRAME_CHROME;
}

bool AppWindowCreateFunction::GetFrameOptions(
    const app_window::CreateWindowOptions& options,
    AppWindow::CreateParams* create_params,
    std::string* error) {
  if (!options.frame)
    return true;

  DCHECK(options.frame->as_string || options.frame->as_frame_options);
  if (options.frame->as_string) {
    create_params->frame = GetFrameFromString(*options.frame->as_string);
    return true;
  }

  if (options.frame->as_frame_options->type)
    create_params->frame =
        GetFrameFromString(*options.frame->as_frame_options->type);

  if (options.frame->as_frame_options->color.get()) {
    if (create_params->frame != AppWindow::FRAME_CHROME) {
      *error = app_window_constants::kColorWithFrameNone;
      return false;
    }

    if (!image_util::ParseHexColorString(
            *options.frame->as_frame_options->color,
            &create_params->active_frame_color)) {
      *error = app_window_constants::kInvalidColorSpecification;
      return false;
    }

    create_params->has_frame_color = true;
    create_params->inactive_frame_color = create_params->active_frame_color;

    if (options.frame->as_frame_options->inactive_color.get()) {
      if (!image_util::ParseHexColorString(
              *options.frame->as_frame_options->inactive_color,
              &create_params->inactive_frame_color)) {
        *error = app_window_constants::kInvalidColorSpecification;
        return false;
      }
    }

    return true;
  }

  if (options.frame->as_frame_options->inactive_color.get()) {
    *error = app_window_constants::kInactiveColorWithoutColor;
    return false;
  }

  return true;
}

}  // namespace extensions

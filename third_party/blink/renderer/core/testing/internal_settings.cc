/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/testing/internal_settings.h"

#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/text/locale_to_script_mapping.h"

#define InternalSettingsGuardForSettingsReturn(returnValue)             \
  if (!settings()) {                                                    \
    exceptionState.throwDOMException(                                   \
        InvalidAccessError, "The settings object cannot be obtained."); \
    return returnValue;                                                 \
  }

#define InternalSettingsGuardForSettings()          \
  if (!GetSettings()) {                             \
    exception_state.ThrowDOMException(              \
        DOMExceptionCode::kInvalidAccessError,      \
        "The settings object cannot be obtained."); \
    return;                                         \
  }

#define InternalSettingsGuardForPage()                                       \
  if (!page()) {                                                             \
    exceptionState.throwDOMException(InvalidAccessError,                     \
                                     "The page object cannot be obtained."); \
    return;                                                                  \
  }

namespace blink {

InternalSettings::Backup::Backup(Settings* settings)
    : original_csp_(RuntimeEnabledFeatures::
                        ExperimentalContentSecurityPolicyFeaturesEnabled()),
      original_editing_behavior_(settings->GetEditingBehaviorType()),
      original_text_autosizing_enabled_(settings->TextAutosizingEnabled()),
      original_text_autosizing_window_size_override_(
          settings->TextAutosizingWindowSizeOverride()),
      original_accessibility_font_scale_factor_(
          settings->GetAccessibilityFontScaleFactor()),
      original_media_type_override_(settings->GetMediaTypeOverride()),
      original_display_mode_override_(settings->GetDisplayModeOverride()),
      original_mock_gesture_tap_highlights_enabled_(
          settings->GetMockGestureTapHighlightsEnabled()),
      lang_attribute_aware_form_control_ui_enabled_(
          RuntimeEnabledFeatures::LangAttributeAwareFormControlUIEnabled()),
      images_enabled_(settings->GetImagesEnabled()),
      default_video_poster_url_(settings->GetDefaultVideoPosterURL()),
      original_image_animation_policy_(settings->GetImageAnimationPolicy()),
      original_scroll_top_left_interop_enabled_(
          RuntimeEnabledFeatures::ScrollTopLeftInteropEnabled()) {}

void InternalSettings::Backup::RestoreTo(Settings* settings) {
  RuntimeEnabledFeatures::SetExperimentalContentSecurityPolicyFeaturesEnabled(
      original_csp_);
  settings->SetEditingBehaviorType(original_editing_behavior_);
  settings->SetTextAutosizingEnabled(original_text_autosizing_enabled_);
  settings->SetTextAutosizingWindowSizeOverride(
      original_text_autosizing_window_size_override_);
  settings->SetAccessibilityFontScaleFactor(
      original_accessibility_font_scale_factor_);
  settings->SetMediaTypeOverride(original_media_type_override_);
  settings->SetDisplayModeOverride(original_display_mode_override_);
  settings->SetMockGestureTapHighlightsEnabled(
      original_mock_gesture_tap_highlights_enabled_);
  RuntimeEnabledFeatures::SetLangAttributeAwareFormControlUIEnabled(
      lang_attribute_aware_form_control_ui_enabled_);
  settings->SetImagesEnabled(images_enabled_);
  settings->SetDefaultVideoPosterURL(default_video_poster_url_);
  settings->GetGenericFontFamilySettings().Reset();
  settings->SetImageAnimationPolicy(original_image_animation_policy_);
  RuntimeEnabledFeatures::SetScrollTopLeftInteropEnabled(
      original_scroll_top_left_interop_enabled_);
}

InternalSettings* InternalSettings::From(Page& page) {
  InternalSettings* supplement = Supplement<Page>::From<InternalSettings>(page);
  if (!supplement) {
    supplement = MakeGarbageCollected<InternalSettings>(page);
    ProvideTo(page, supplement);
  }
  return supplement;
}

InternalSettings::~InternalSettings() = default;

InternalSettings::InternalSettings(Page& page)
    : InternalSettingsGenerated(&page),
      InternalSettingsPageSupplementBase(page),
      backup_(&page.GetSettings()) {}

void InternalSettings::ResetToConsistentState() {
  backup_.RestoreTo(GetSettings());
  backup_ = Backup(GetSettings());
  backup_.original_text_autosizing_enabled_ =
      GetSettings()->TextAutosizingEnabled();

  InternalSettingsGenerated::resetToConsistentState();
}

Settings* InternalSettings::GetSettings() const {
  if (!GetPage())
    return nullptr;
  return &GetPage()->GetSettings();
}

void InternalSettings::setHideScrollbars(bool enabled,
                                         ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  GetSettings()->SetHideScrollbars(enabled);
}

void InternalSettings::setMockGestureTapHighlightsEnabled(
    bool enabled,
    ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  GetSettings()->SetMockGestureTapHighlightsEnabled(enabled);
}

void InternalSettings::setExperimentalContentSecurityPolicyFeaturesEnabled(
    bool enabled) {
  RuntimeEnabledFeatures::SetExperimentalContentSecurityPolicyFeaturesEnabled(
      enabled);
}

void InternalSettings::setViewportEnabled(bool enabled,
                                          ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  GetSettings()->SetViewportEnabled(enabled);
}

void InternalSettings::setViewportMetaEnabled(bool enabled,
                                              ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  GetSettings()->SetViewportMetaEnabled(enabled);
}

void InternalSettings::setViewportStyle(const String& style,
                                        ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  if (DeprecatedEqualIgnoringCase(style, "default"))
    GetSettings()->SetViewportStyle(WebViewportStyle::kDefault);
  else if (DeprecatedEqualIgnoringCase(style, "mobile"))
    GetSettings()->SetViewportStyle(WebViewportStyle::kMobile);
  else if (DeprecatedEqualIgnoringCase(style, "television"))
    GetSettings()->SetViewportStyle(WebViewportStyle::kTelevision);
  else
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The viewport style type provided ('" + style + "') is invalid.");
}

void InternalSettings::setStandardFontFamily(const AtomicString& family,
                                             const String& script,
                                             ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  UScriptCode code = ScriptNameToCode(script);
  if (code == USCRIPT_INVALID_CODE)
    return;
  if (GetSettings()->GetGenericFontFamilySettings().UpdateStandard(family,
                                                                   code))
    GetSettings()->NotifyGenericFontFamilyChange();
}

void InternalSettings::setSerifFontFamily(const AtomicString& family,
                                          const String& script,
                                          ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  UScriptCode code = ScriptNameToCode(script);
  if (code == USCRIPT_INVALID_CODE)
    return;
  if (GetSettings()->GetGenericFontFamilySettings().UpdateSerif(family, code))
    GetSettings()->NotifyGenericFontFamilyChange();
}

void InternalSettings::setSansSerifFontFamily(const AtomicString& family,
                                              const String& script,
                                              ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  UScriptCode code = ScriptNameToCode(script);
  if (code == USCRIPT_INVALID_CODE)
    return;
  if (GetSettings()->GetGenericFontFamilySettings().UpdateSansSerif(family,
                                                                    code))
    GetSettings()->NotifyGenericFontFamilyChange();
}

void InternalSettings::setFixedFontFamily(const AtomicString& family,
                                          const String& script,
                                          ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  UScriptCode code = ScriptNameToCode(script);
  if (code == USCRIPT_INVALID_CODE)
    return;
  if (GetSettings()->GetGenericFontFamilySettings().UpdateFixed(family, code))
    GetSettings()->NotifyGenericFontFamilyChange();
}

void InternalSettings::setCursiveFontFamily(const AtomicString& family,
                                            const String& script,
                                            ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  UScriptCode code = ScriptNameToCode(script);
  if (code == USCRIPT_INVALID_CODE)
    return;
  if (GetSettings()->GetGenericFontFamilySettings().UpdateCursive(family, code))
    GetSettings()->NotifyGenericFontFamilyChange();
}

void InternalSettings::setFantasyFontFamily(const AtomicString& family,
                                            const String& script,
                                            ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  UScriptCode code = ScriptNameToCode(script);
  if (code == USCRIPT_INVALID_CODE)
    return;
  if (GetSettings()->GetGenericFontFamilySettings().UpdateFantasy(family, code))
    GetSettings()->NotifyGenericFontFamilyChange();
}

void InternalSettings::setPictographFontFamily(
    const AtomicString& family,
    const String& script,
    ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  UScriptCode code = ScriptNameToCode(script);
  if (code == USCRIPT_INVALID_CODE)
    return;
  if (GetSettings()->GetGenericFontFamilySettings().UpdatePictograph(family,
                                                                     code))
    GetSettings()->NotifyGenericFontFamilyChange();
}

void InternalSettings::setTextAutosizingEnabled(
    bool enabled,
    ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  GetSettings()->SetTextAutosizingEnabled(enabled);
}

void InternalSettings::setTextAutosizingWindowSizeOverride(
    int width,
    int height,
    ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  GetSettings()->SetTextAutosizingWindowSizeOverride(IntSize(width, height));
}

void InternalSettings::setTextTrackKindUserPreference(
    const String& preference,
    ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  String token = preference.StripWhiteSpace();
  TextTrackKindUserPreference user_preference =
      TextTrackKindUserPreference::kDefault;
  if (token == "default") {
    user_preference = TextTrackKindUserPreference::kDefault;
  } else if (token == "captions") {
    user_preference = TextTrackKindUserPreference::kCaptions;
  } else if (token == "subtitles") {
    user_preference = TextTrackKindUserPreference::kSubtitles;
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The user preference for text track kind " + preference +
            ")' is invalid.");
    return;
  }

  GetSettings()->SetTextTrackKindUserPreference(user_preference);
}

void InternalSettings::setMediaTypeOverride(const String& media_type,
                                            ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  GetSettings()->SetMediaTypeOverride(media_type);
}

void InternalSettings::setAccessibilityFontScaleFactor(
    float font_scale_factor,
    ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  GetSettings()->SetAccessibilityFontScaleFactor(font_scale_factor);
}

void InternalSettings::setEditingBehavior(const String& editing_behavior,
                                          ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  if (DeprecatedEqualIgnoringCase(editing_behavior, "win"))
    GetSettings()->SetEditingBehaviorType(kEditingWindowsBehavior);
  else if (DeprecatedEqualIgnoringCase(editing_behavior, "mac"))
    GetSettings()->SetEditingBehaviorType(kEditingMacBehavior);
  else if (DeprecatedEqualIgnoringCase(editing_behavior, "unix"))
    GetSettings()->SetEditingBehaviorType(kEditingUnixBehavior);
  else if (DeprecatedEqualIgnoringCase(editing_behavior, "android"))
    GetSettings()->SetEditingBehaviorType(kEditingAndroidBehavior);
  else
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "The editing behavior type provided ('" +
                                          editing_behavior + "') is invalid.");
}

void InternalSettings::setLangAttributeAwareFormControlUIEnabled(bool enabled) {
  RuntimeEnabledFeatures::SetLangAttributeAwareFormControlUIEnabled(enabled);
}

void InternalSettings::setImagesEnabled(bool enabled,
                                        ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  GetSettings()->SetImagesEnabled(enabled);
}

void InternalSettings::setDefaultVideoPosterURL(
    const String& url,
    ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  GetSettings()->SetDefaultVideoPosterURL(url);
}

void InternalSettings::Trace(blink::Visitor* visitor) {
  InternalSettingsGenerated::Trace(visitor);
  Supplement<Page>::Trace(visitor);
}

void InternalSettings::setAvailablePointerTypes(
    const String& pointers,
    ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();

  // Allow setting multiple pointer types by passing comma seperated list
  // ("coarse,fine").
  Vector<String> tokens;
  pointers.Split(",", false, tokens);

  int pointer_types = 0;
  for (const String& split_token : tokens) {
    String token = split_token.StripWhiteSpace();

    if (token == "coarse") {
      pointer_types |= kPointerTypeCoarse;
    } else if (token == "fine") {
      pointer_types |= kPointerTypeFine;
    } else if (token == "none") {
      pointer_types |= kPointerTypeNone;
    } else {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "The pointer type token ('" + token + ")' is invalid.");
      return;
    }
  }

  GetSettings()->SetAvailablePointerTypes(pointer_types);
}

void InternalSettings::setDisplayModeOverride(const String& display_mode,
                                              ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  String token = display_mode.StripWhiteSpace();

  auto mode = blink::mojom::DisplayMode::kBrowser;
  if (token == "browser") {
    mode = blink::mojom::DisplayMode::kBrowser;
  } else if (token == "minimal-ui") {
    mode = blink::mojom::DisplayMode::kMinimalUi;
  } else if (token == "standalone") {
    mode = blink::mojom::DisplayMode::kStandalone;
  } else if (token == "fullscreen") {
    mode = blink::mojom::DisplayMode::kFullscreen;
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The display-mode token ('" + token + ")' is invalid.");
    return;
  }

  GetSettings()->SetDisplayModeOverride(mode);
}

void InternalSettings::setPrimaryPointerType(const String& pointer,
                                             ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  String token = pointer.StripWhiteSpace();

  PointerType type = kPointerTypeNone;
  if (token == "coarse") {
    type = kPointerTypeCoarse;
  } else if (token == "fine") {
    type = kPointerTypeFine;
  } else if (token == "none") {
    type = kPointerTypeNone;
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The pointer type token ('" + token + ")' is invalid.");
    return;
  }

  GetSettings()->SetPrimaryPointerType(type);
}

void InternalSettings::setAvailableHoverTypes(const String& types,
                                              ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();

  // Allow setting multiple hover types by passing comma seperated list
  // ("on-demand,none").
  Vector<String> tokens;
  types.Split(",", false, tokens);

  int hover_types = 0;
  for (const String& split_token : tokens) {
    String token = split_token.StripWhiteSpace();
    if (token == "none") {
      hover_types |= kHoverTypeNone;
    } else if (token == "hover") {
      hover_types |= kHoverTypeHover;
    } else {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "The hover type token ('" + token + ")' is invalid.");
      return;
    }
  }

  GetSettings()->SetAvailableHoverTypes(hover_types);
}

void InternalSettings::setPrimaryHoverType(const String& type,
                                           ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  String token = type.StripWhiteSpace();

  HoverType hover_type = kHoverTypeNone;
  if (token == "none") {
    hover_type = kHoverTypeNone;
  } else if (token == "hover") {
    hover_type = kHoverTypeHover;
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The hover type token ('" + token + ")' is invalid.");
    return;
  }

  GetSettings()->SetPrimaryHoverType(hover_type);
}

void InternalSettings::setImageAnimationPolicy(
    const String& policy,
    ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  if (DeprecatedEqualIgnoringCase(policy, "allowed")) {
    GetSettings()->SetImageAnimationPolicy(kImageAnimationPolicyAllowed);
  } else if (DeprecatedEqualIgnoringCase(policy, "once")) {
    GetSettings()->SetImageAnimationPolicy(kImageAnimationPolicyAnimateOnce);
  } else if (DeprecatedEqualIgnoringCase(policy, "none")) {
    GetSettings()->SetImageAnimationPolicy(kImageAnimationPolicyNoAnimation);
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The image animation policy provided ('" + policy + "') is invalid.");
    return;
  }
}

void InternalSettings::setScrollTopLeftInteropEnabled(bool enabled) {
  RuntimeEnabledFeatures::SetScrollTopLeftInteropEnabled(enabled);
}

void InternalSettings::SetDnsPrefetchLogging(bool enabled,
                                             ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  GetSettings()->SetLogDnsPrefetchAndPreconnect(enabled);
}

void InternalSettings::SetPreloadLogging(bool enabled,
                                         ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  GetSettings()->SetLogPreload(enabled);
}

void InternalSettings::setPresentationReceiver(
    bool enabled,
    ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  GetSettings()->SetPresentationReceiver(enabled);
}

void InternalSettings::setAutoplayPolicy(const String& policy_str,
                                         ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();

  AutoplayPolicy::Type policy = AutoplayPolicy::Type::kNoUserGestureRequired;
  if (policy_str == "no-user-gesture-required") {
    policy = AutoplayPolicy::Type::kNoUserGestureRequired;
  } else if (policy_str == "user-gesture-required") {
    policy = AutoplayPolicy::Type::kUserGestureRequired;
  } else if (policy_str == "document-user-activation-required") {
    policy = AutoplayPolicy::Type::kDocumentUserActivationRequired;
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The autoplay policy ('" + policy_str + ")' is invalid.");
  }

  GetSettings()->SetAutoplayPolicy(policy);
}

void InternalSettings::setUniversalAccessFromFileURLs(
    bool enabled,
    ExceptionState& exception_state) {
  InternalSettingsGuardForSettings();
  GetSettings()->SetAllowUniversalAccessFromFileURLs(enabled);
}

}  // namespace blink

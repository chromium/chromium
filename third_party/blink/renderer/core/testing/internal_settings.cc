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
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/text/locale_to_script_mapping.h"

namespace blink {

using mojom::blink::HoverType;
using mojom::blink::PointerType;

InternalSettings* InternalSettings::From(Page& page) {
  InternalSettings* supplement = Supplement<Page>::From<InternalSettings>(page);
  if (!supplement) {
    supplement = MakeGarbageCollected<InternalSettings>(page);
    ProvideTo(page, supplement);
  }
  return supplement;
}

InternalSettings::InternalSettings(Page& page)
    : InternalSettingsGenerated(page),
      generic_font_family_settings_backup_(
          GetSettings().GetGenericFontFamilySettings()) {}

InternalSettings::~InternalSettings() = default;

void InternalSettings::ResetToConsistentState() {
  InternalSettingsGenerated::ResetToConsistentState();
  GetSettings().GetGenericFontFamilySettings() =
      generic_font_family_settings_backup_;
}

void InternalSettings::setViewportStyle(const String& style,
                                        ExceptionState& exception_state) {
  if (EqualIgnoringASCIICase(style, "default")) {
    GetSettings().SetViewportStyle(mojom::blink::ViewportStyle::kDefault);
  } else if (EqualIgnoringASCIICase(style, "mobile")) {
    GetSettings().SetViewportStyle(mojom::blink::ViewportStyle::kMobile);
  } else if (EqualIgnoringASCIICase(style, "television")) {
    GetSettings().SetViewportStyle(mojom::blink::ViewportStyle::kTelevision);
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The viewport style type provided ('" + style + "') is invalid.");
  }
}

void InternalSettings::SetFontFamily(
    const AtomicString& family,
    const String& script,
    bool (GenericFontFamilySettings::*update_method)(const AtomicString&,
                                                     UScriptCode)) {
  UScriptCode code = ScriptNameToCode(script);
  if (code == USCRIPT_INVALID_CODE) {
    return;
  }
  if ((GetSettings().GetGenericFontFamilySettings().*update_method)(family,
                                                                    code)) {
    GetSettings().NotifyGenericFontFamilyChange();
  }
}

void InternalSettings::setStandardFontFamily(const AtomicString& family,
                                             const String& script) {
  SetFontFamily(family, script, &GenericFontFamilySettings::UpdateStandard);
}

void InternalSettings::setSerifFontFamily(const AtomicString& family,
                                          const String& script) {
  SetFontFamily(family, script, &GenericFontFamilySettings::UpdateSerif);
}

void InternalSettings::setSansSerifFontFamily(const AtomicString& family,
                                              const String& script) {
  SetFontFamily(family, script, &GenericFontFamilySettings::UpdateSansSerif);
}

void InternalSettings::setFixedFontFamily(const AtomicString& family,
                                          const String& script) {
  SetFontFamily(family, script, &GenericFontFamilySettings::UpdateFixed);
}

void InternalSettings::setCursiveFontFamily(const AtomicString& family,
                                            const String& script) {
  SetFontFamily(family, script, &GenericFontFamilySettings::UpdateCursive);
}

void InternalSettings::setFantasyFontFamily(const AtomicString& family,
                                            const String& script) {
  SetFontFamily(family, script, &GenericFontFamilySettings::UpdateFantasy);
}

void InternalSettings::setMathFontFamily(const AtomicString& family,
                                         const String& script) {
  SetFontFamily(family, script, &GenericFontFamilySettings::UpdateMath);
}

void InternalSettings::setTextAutosizingWindowSizeOverride(int width,
                                                           int height) {
  GetSettings().SetTextAutosizingWindowSizeOverride(gfx::Size(width, height));
}

void InternalSettings::setTextTrackKindUserPreference(
    const String& preference,
    ExceptionState& exception_state) {
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

  GetSettings().SetTextTrackKindUserPreference(user_preference);
}

void InternalSettings::setEditingBehavior(const String& editing_behavior,
                                          ExceptionState& exception_state) {
  if (EqualIgnoringASCIICase(editing_behavior, "win")) {
    GetSettings().SetEditingBehaviorType(
        mojom::EditingBehavior::kEditingWindowsBehavior);
  } else if (EqualIgnoringASCIICase(editing_behavior, "mac")) {
    GetSettings().SetEditingBehaviorType(
        mojom::EditingBehavior::kEditingMacBehavior);
  } else if (EqualIgnoringASCIICase(editing_behavior, "unix")) {
    GetSettings().SetEditingBehaviorType(
        mojom::EditingBehavior::kEditingUnixBehavior);
  } else if (EqualIgnoringASCIICase(editing_behavior, "android")) {
    GetSettings().SetEditingBehaviorType(
        mojom::EditingBehavior::kEditingAndroidBehavior);
  } else if (EqualIgnoringASCIICase(editing_behavior, "chromeos")) {
    GetSettings().SetEditingBehaviorType(
        mojom::EditingBehavior::kEditingChromeOSBehavior);
  } else {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "The editing behavior type provided ('" +
                                          editing_behavior + "') is invalid.");
  }
}

void InternalSettings::setAvailablePointerTypes(
    const String& pointers,
    ExceptionState& exception_state) {
  // Allow setting multiple pointer types by passing comma seperated list
  // ("coarse,fine").
  Vector<String> tokens;
  pointers.Split(",", false, tokens);

  int pointer_types = 0;
  for (const String& split_token : tokens) {
    String token = split_token.StripWhiteSpace();

    if (token == "coarse") {
      pointer_types |= static_cast<int>(PointerType::kPointerCoarseType);
    } else if (token == "fine") {
      pointer_types |= static_cast<int>(PointerType::kPointerFineType);
    } else if (token == "none") {
      pointer_types |= static_cast<int>(PointerType::kPointerNone);
    } else {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "The pointer type token ('" + token + ")' is invalid.");
      return;
    }
  }

  GetSettings().SetAvailablePointerTypes(pointer_types);
}

void InternalSettings::setDisplayModeOverride(const String& display_mode,
                                              ExceptionState& exception_state) {
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
  } else if (token == "window-controls-overlay") {
    mode = blink::mojom::DisplayMode::kWindowControlsOverlay;
  } else if (token == "borderless") {
    mode = blink::mojom::DisplayMode::kBorderless;
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The display-mode token ('" + token + ")' is invalid.");
    return;
  }

  GetSettings().SetDisplayModeOverride(mode);
}

void InternalSettings::setPrimaryPointerType(const String& pointer,
                                             ExceptionState& exception_state) {
  String token = pointer.StripWhiteSpace();
  PointerType type = PointerType::kPointerNone;
  if (token == "coarse") {
    type = PointerType::kPointerCoarseType;
  } else if (token == "fine") {
    type = PointerType::kPointerFineType;
  } else if (token == "none") {
    type = PointerType::kPointerNone;
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The pointer type token ('" + token + ")' is invalid.");
    return;
  }

  GetSettings().SetPrimaryPointerType(type);
}

void InternalSettings::setAvailableHoverTypes(const String& types,
                                              ExceptionState& exception_state) {
  // Allow setting multiple hover types by passing comma seperated list
  // ("on-demand,none").
  Vector<String> tokens;
  types.Split(",", false, tokens);

  int hover_types = 0;
  for (const String& split_token : tokens) {
    String token = split_token.StripWhiteSpace();
    if (token == "none") {
      hover_types |= static_cast<int>(HoverType::kHoverNone);
    } else if (token == "hover") {
      hover_types |= static_cast<int>(HoverType::kHoverHoverType);
    } else {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "The hover type token ('" + token + ")' is invalid.");
      return;
    }
  }

  GetSettings().SetAvailableHoverTypes(hover_types);
}

void InternalSettings::setPrimaryHoverType(const String& type,
                                           ExceptionState& exception_state) {
  String token = type.StripWhiteSpace();
  HoverType hover_type = HoverType::kHoverNone;
  if (token == "none") {
    hover_type = HoverType::kHoverNone;
  } else if (token == "hover") {
    hover_type = HoverType::kHoverHoverType;
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The hover type token ('" + token + ")' is invalid.");
    return;
  }

  GetSettings().SetPrimaryHoverType(hover_type);
}

void InternalSettings::setImageAnimationPolicy(
    const String& policy,
    ExceptionState& exception_state) {
  if (EqualIgnoringASCIICase(policy, "allowed")) {
    GetSettings().SetImageAnimationPolicy(
        mojom::blink::ImageAnimationPolicy::kImageAnimationPolicyAllowed);
  } else if (EqualIgnoringASCIICase(policy, "once")) {
    GetSettings().SetImageAnimationPolicy(
        mojom::blink::ImageAnimationPolicy::kImageAnimationPolicyAnimateOnce);
  } else if (EqualIgnoringASCIICase(policy, "none")) {
    GetSettings().SetImageAnimationPolicy(
        mojom::blink::ImageAnimationPolicy::kImageAnimationPolicyNoAnimation);
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The image animation policy provided ('" + policy + "') is invalid.");
    return;
  }
}

void InternalSettings::setAutoplayPolicy(const String& policy_str,
                                         ExceptionState& exception_state) {
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

  GetSettings().SetAutoplayPolicy(policy);
}

void InternalSettings::setPreferCompositingToLCDTextEnabled(bool enabled) {
  GetSettings().SetPreferCompositingToLCDTextForTesting(enabled);
}

}  // namespace blink

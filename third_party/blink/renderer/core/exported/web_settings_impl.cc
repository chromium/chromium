/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/exported/web_settings_impl.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/platform/graphics/deferred_image_decoder.h"

namespace blink {

WebSettingsImpl::WebSettingsImpl(Settings* settings,
                                 DevToolsEmulator* dev_tools_emulator)
    : settings_(settings),
      dev_tools_emulator_(dev_tools_emulator),
      render_v_sync_notification_enabled_(false),
      auto_zoom_focused_editable_to_legible_scale_(false),
      support_deprecated_target_density_dpi_(false),
      viewport_meta_non_user_scalable_quirk_(false),
      clobber_user_agent_initial_scale_quirk_(false) {
  DCHECK(settings);
}

void WebSettingsImpl::SetFromStrings(const WebString& name,
                                     const WebString& value) {
  settings_->SetFromStrings(name, value);
}

void WebSettingsImpl::SetStandardFontFamily(const WebString& font,
                                            UScriptCode script) {
  if (settings_->GetGenericFontFamilySettings().UpdateStandard(font, script))
    settings_->NotifyGenericFontFamilyChange();
}

void WebSettingsImpl::SetFixedFontFamily(const WebString& font,
                                         UScriptCode script) {
  if (settings_->GetGenericFontFamilySettings().UpdateFixed(font, script))
    settings_->NotifyGenericFontFamilyChange();
}

void WebSettingsImpl::SetNetworkQuietTimeout(double timeout) {
  settings_->SetNetworkQuietTimeout(timeout);
}

void WebSettingsImpl::SetForceMainWorldInitialization(bool enabled) {
  settings_->SetForceMainWorldInitialization(enabled);
}

void WebSettingsImpl::SetForceZeroLayoutHeight(bool enabled) {
  settings_->SetForceZeroLayoutHeight(enabled);
}

void WebSettingsImpl::SetFullscreenSupported(bool enabled) {
  settings_->SetFullscreenSupported(enabled);
}

void WebSettingsImpl::SetSerifFontFamily(const WebString& font,
                                         UScriptCode script) {
  if (settings_->GetGenericFontFamilySettings().UpdateSerif(font, script))
    settings_->NotifyGenericFontFamilyChange();
}

void WebSettingsImpl::SetSansSerifFontFamily(const WebString& font,
                                             UScriptCode script) {
  if (settings_->GetGenericFontFamilySettings().UpdateSansSerif(font, script))
    settings_->NotifyGenericFontFamilyChange();
}

void WebSettingsImpl::SetCursiveFontFamily(const WebString& font,
                                           UScriptCode script) {
  if (settings_->GetGenericFontFamilySettings().UpdateCursive(font, script))
    settings_->NotifyGenericFontFamilyChange();
}

void WebSettingsImpl::SetFantasyFontFamily(const WebString& font,
                                           UScriptCode script) {
  if (settings_->GetGenericFontFamilySettings().UpdateFantasy(font, script))
    settings_->NotifyGenericFontFamilyChange();
}

void WebSettingsImpl::SetMathFontFamily(const WebString& font,
                                        UScriptCode script) {
  if (settings_->GetGenericFontFamilySettings().UpdateMath(font, script))
    settings_->NotifyGenericFontFamilyChange();
}

void WebSettingsImpl::SetDefaultFontSize(int size) {
  settings_->SetDefaultFontSize(size);
}

void WebSettingsImpl::SetDefaultFixedFontSize(int size) {
  settings_->SetDefaultFixedFontSize(size);
}

void WebSettingsImpl::SetDefaultVideoPosterURL(const WebString& url) {
  settings_->SetDefaultVideoPosterURL(url);
}

void WebSettingsImpl::SetMinimumFontSize(int size) {
  settings_->SetMinimumFontSize(size);
}

void WebSettingsImpl::SetMinimumLogicalFontSize(int size) {
  settings_->SetMinimumLogicalFontSize(size);
}

void WebSettingsImpl::SetAutoplayPolicy(mojom::blink::AutoplayPolicy policy) {
  settings_->SetAutoplayPolicy(
      static_cast<blink::AutoplayPolicy::Type>(policy));
}

void WebSettingsImpl::SetRequireTransientActivationForGetDisplayMedia(
    bool required) {
  settings_->SetRequireTransientActivationForGetDisplayMedia(required);
}

void WebSettingsImpl::SetRequireTransientActivationForShowFileOrDirectoryPicker(
    bool required) {
  settings_->SetRequireTransientActivationForShowFileOrDirectoryPicker(
      required);
}

void WebSettingsImpl::SetAutoZoomFocusedEditableToLegibleScale(
    bool auto_zoom_focused_editable_to_legible_scale) {
  auto_zoom_focused_editable_to_legible_scale_ =
      auto_zoom_focused_editable_to_legible_scale;
}

void WebSettingsImpl::SetTextAutosizingEnabled(bool enabled) {
  dev_tools_emulator_->SetTextAutosizingEnabled(enabled);
}

void WebSettingsImpl::SetAccessibilityFontScaleFactor(float font_scale_factor) {
  settings_->SetAccessibilityFontScaleFactor(font_scale_factor);
}

void WebSettingsImpl::SetAccessibilityTextSizeContrastFactor(
    int text_size_contrast_factor) {
  settings_->SetAccessibilityTextSizeContrastFactor(text_size_contrast_factor);
}

void WebSettingsImpl::SetAccessibilityAlwaysShowFocus(bool always_show_focus) {
  settings_->SetAccessibilityAlwaysShowFocus(always_show_focus);
}

void WebSettingsImpl::SetAccessibilityPasswordValuesEnabled(bool enabled) {
  settings_->SetAccessibilityPasswordValuesEnabled(enabled);
}

void WebSettingsImpl::SetAccessibilityFontWeightAdjustment(int size) {
  settings_->SetAccessibilityFontWeightAdjustment(size);
}

void WebSettingsImpl::SetDeviceScaleAdjustment(float device_scale_adjustment) {
  dev_tools_emulator_->SetDeviceScaleAdjustment(device_scale_adjustment);
}

void WebSettingsImpl::SetDefaultTextEncodingName(const WebString& encoding) {
  settings_->SetDefaultTextEncodingName((String)encoding);
}

void WebSettingsImpl::SetJavaScriptEnabled(bool enabled) {
  dev_tools_emulator_->SetScriptEnabled(enabled);
}

void WebSettingsImpl::SetWebSecurityEnabled(bool enabled) {
  settings_->SetWebSecurityEnabled(enabled);
}

void WebSettingsImpl::SetSupportDeprecatedTargetDensityDPI(
    bool support_deprecated_target_density_dpi) {
  support_deprecated_target_density_dpi_ =
      support_deprecated_target_density_dpi;
}

void WebSettingsImpl::SetViewportMetaMergeContentQuirk(
    bool viewport_meta_merge_content_quirk) {
  settings_->SetViewportMetaMergeContentQuirk(
      viewport_meta_merge_content_quirk);
}

void WebSettingsImpl::SetViewportMetaNonUserScalableQuirk(
    bool viewport_meta_non_user_scalable_quirk) {
  viewport_meta_non_user_scalable_quirk_ =
      viewport_meta_non_user_scalable_quirk;
}

void WebSettingsImpl::SetViewportMetaZeroValuesQuirk(
    bool viewport_meta_zero_values_quirk) {
  settings_->SetViewportMetaZeroValuesQuirk(viewport_meta_zero_values_quirk);
}

void WebSettingsImpl::SetIgnoreMainFrameOverflowHiddenQuirk(
    bool ignore_main_frame_overflow_hidden_quirk) {
  settings_->SetIgnoreMainFrameOverflowHiddenQuirk(
      ignore_main_frame_overflow_hidden_quirk);
}

void WebSettingsImpl::SetReportScreenSizeInPhysicalPixelsQuirk(
    bool report_screen_size_in_physical_pixels_quirk) {
  settings_->SetReportScreenSizeInPhysicalPixelsQuirk(
      report_screen_size_in_physical_pixels_quirk);
}

void WebSettingsImpl::SetRubberBandingOnCompositorThread(
    bool rubber_banding_on_compositor_thread) {}

void WebSettingsImpl::SetClobberUserAgentInitialScaleQuirk(
    bool clobber_user_agent_initial_scale_quirk) {
  clobber_user_agent_initial_scale_quirk_ =
      clobber_user_agent_initial_scale_quirk;
}

void WebSettingsImpl::SetSupportsMultipleWindows(
    bool supports_multiple_windows) {
  settings_->SetSupportsMultipleWindows(supports_multiple_windows);
}

void WebSettingsImpl::SetLoadsImagesAutomatically(
    bool loads_images_automatically) {
  settings_->SetLoadsImagesAutomatically(loads_images_automatically);
}

void WebSettingsImpl::SetImageAnimationPolicy(
    mojom::blink::ImageAnimationPolicy policy) {
  settings_->SetImageAnimationPolicy(policy);
}

void WebSettingsImpl::SetImagesEnabled(bool enabled) {
  settings_->SetImagesEnabled(enabled);
}

void WebSettingsImpl::SetLoadWithOverviewMode(bool enabled) {
  settings_->SetLoadWithOverviewMode(enabled);
}

void WebSettingsImpl::SetShouldReuseGlobalForUnownedMainFrame(bool enabled) {
  settings_->SetShouldReuseGlobalForUnownedMainFrame(enabled);
}

void WebSettingsImpl::SetPluginsEnabled(bool enabled) {
  dev_tools_emulator_->SetPluginsEnabled(enabled);
}

void WebSettingsImpl::SetAvailablePointerTypes(int pointers) {
  dev_tools_emulator_->SetAvailablePointerTypes(pointers);
}

void WebSettingsImpl::SetPrimaryPointerType(mojom::blink::PointerType pointer) {
  dev_tools_emulator_->SetPrimaryPointerType(pointer);
}

void WebSettingsImpl::SetAvailableHoverTypes(int types) {
  dev_tools_emulator_->SetAvailableHoverTypes(types);
}

void WebSettingsImpl::SetPrimaryHoverType(mojom::blink::HoverType type) {
  dev_tools_emulator_->SetPrimaryHoverType(type);
}

void WebSettingsImpl::SetOutputDeviceUpdateAbilityType(
    mojom::blink::OutputDeviceUpdateAbilityType type) {
  dev_tools_emulator_->SetOutputDeviceUpdateAbilityType(type);
}

void WebSettingsImpl::SetPreferHiddenVolumeControls(bool enabled) {
  settings_->SetPreferHiddenVolumeControls(enabled);
}

void WebSettingsImpl::SetShouldProtectAgainstIpcFlooding(bool enabled) {
  settings_->SetShouldProtectAgainstIpcFlooding(enabled);
}

void WebSettingsImpl::SetDOMPasteAllowed(bool enabled) {
  settings_->SetDOMPasteAllowed(enabled);
}

void WebSettingsImpl::SetShrinksViewportContentToFit(
    bool shrink_viewport_content) {
  dev_tools_emulator_->SetShrinksViewportContentToFit(shrink_viewport_content);
}

void WebSettingsImpl::SetSpatialNavigationEnabled(bool enabled) {
  settings_->SetSpatialNavigationEnabled(enabled);
}

void WebSettingsImpl::SetSpellCheckEnabledByDefault(bool enabled) {
  settings_->SetSpellCheckEnabledByDefault(enabled);
}

void WebSettingsImpl::SetTextAreasAreResizable(bool are_resizable) {
  settings_->SetTextAreasAreResizable(are_resizable);
}

void WebSettingsImpl::SetAllowScriptsToCloseWindows(bool allow) {
  settings_->SetAllowScriptsToCloseWindows(allow);
}

void WebSettingsImpl::SetWideViewportQuirkEnabled(
    bool wide_viewport_quirk_enabled) {
  settings_->SetWideViewportQuirkEnabled(wide_viewport_quirk_enabled);
}

void WebSettingsImpl::SetUseWideViewport(bool use_wide_viewport) {
  settings_->SetUseWideViewport(use_wide_viewport);
}

void WebSettingsImpl::SetDontSendKeyEventsToJavascript(
    bool dont_send_key_events_to_javascript) {
  settings_->SetDontSendKeyEventsToJavascript(
      dont_send_key_events_to_javascript);
}

void WebSettingsImpl::SetDoubleTapToZoomEnabled(
    bool double_tap_to_zoom_enabled) {
  dev_tools_emulator_->SetDoubleTapToZoomEnabled(double_tap_to_zoom_enabled);
}

void WebSettingsImpl::SetDownloadableBinaryFontsEnabled(bool enabled) {
  settings_->SetDownloadableBinaryFontsEnabled(enabled);
}

void WebSettingsImpl::SetJavaScriptCanAccessClipboard(bool enabled) {
  settings_->SetJavaScriptCanAccessClipboard(enabled);
}

void WebSettingsImpl::SetTextTrackKindUserPreference(
    TextTrackKindUserPreference preference) {
  settings_->SetTextTrackKindUserPreference(
      static_cast<blink::TextTrackKindUserPreference>(preference));
}

void WebSettingsImpl::SetTextTrackBackgroundColor(const WebString& color) {
  settings_->SetTextTrackBackgroundColor(color);
}

void WebSettingsImpl::SetTextTrackFontFamily(const WebString& font_family) {
  settings_->SetTextTrackFontFamily(font_family);
}

void WebSettingsImpl::SetTextTrackFontStyle(const WebString& font_style) {
  settings_->SetTextTrackFontStyle(font_style);
}

void WebSettingsImpl::SetTextTrackFontVariant(const WebString& font_variant) {
  settings_->SetTextTrackFontVariant(font_variant);
}

void WebSettingsImpl::SetTextTrackMarginPercentage(float percentage) {
  settings_->SetTextTrackMarginPercentage(percentage);
}

void WebSettingsImpl::SetTextTrackTextColor(const WebString& color) {
  settings_->SetTextTrackTextColor(color);
}

void WebSettingsImpl::SetTextTrackTextShadow(const WebString& shadow) {
  settings_->SetTextTrackTextShadow(shadow);
}

void WebSettingsImpl::SetTextTrackTextSize(const WebString& size) {
  settings_->SetTextTrackTextSize(size);
}

void WebSettingsImpl::SetTextTrackWindowColor(const WebString& color) {
  settings_->SetTextTrackWindowColor(color);
}

void WebSettingsImpl::SetTextTrackWindowRadius(const WebString& radius) {
  settings_->SetTextTrackWindowRadius(radius);
}

void WebSettingsImpl::SetDNSPrefetchingEnabled(bool enabled) {
  settings_->SetDNSPrefetchingEnabled(enabled);
}

void WebSettingsImpl::SetLocalStorageEnabled(bool enabled) {
  settings_->SetLocalStorageEnabled(enabled);
}

void WebSettingsImpl::SetMainFrameClipsContent(bool enabled) {
  settings_->SetMainFrameClipsContent(enabled);
}

void WebSettingsImpl::SetMaxTouchPoints(int max_touch_points) {
  settings_->SetMaxTouchPoints(max_touch_points);
}

void WebSettingsImpl::SetAllowUniversalAccessFromFileURLs(bool allow) {
  settings_->SetAllowUniversalAccessFromFileURLs(allow);
}

void WebSettingsImpl::SetAllowFileAccessFromFileURLs(bool allow) {
  settings_->SetAllowFileAccessFromFileURLs(allow);
}

void WebSettingsImpl::SetAllowGeolocationOnInsecureOrigins(bool allow) {
  settings_->SetAllowGeolocationOnInsecureOrigins(allow);
}

void WebSettingsImpl::SetTouchDragDropEnabled(bool enabled) {
  settings_->SetTouchDragDropEnabled(enabled);
}

void WebSettingsImpl::SetTouchDragEndContextMenu(bool enabled) {
  settings_->SetTouchDragEndContextMenu(enabled);
}

void WebSettingsImpl::SetBarrelButtonForDragEnabled(bool enabled) {
  settings_->SetBarrelButtonForDragEnabled(enabled);
}

void WebSettingsImpl::SetWebGL1Enabled(bool enabled) {
  settings_->SetWebGL1Enabled(enabled);
}

void WebSettingsImpl::SetWebGL2Enabled(bool enabled) {
  settings_->SetWebGL2Enabled(enabled);
}

void WebSettingsImpl::SetRenderVSyncNotificationEnabled(bool enabled) {
  render_v_sync_notification_enabled_ = enabled;
}

void WebSettingsImpl::SetWebGLErrorsToConsoleEnabled(bool enabled) {
  settings_->SetWebGLErrorsToConsoleEnabled(enabled);
}

void WebSettingsImpl::SetAlwaysShowContextMenuOnTouch(bool enabled) {
  settings_->SetAlwaysShowContextMenuOnTouch(enabled);
}

void WebSettingsImpl::SetSmoothScrollForFindEnabled(bool enabled) {
  settings_->SetSmoothScrollForFindEnabled(enabled);
}

void WebSettingsImpl::SetShowContextMenuOnMouseUp(bool enabled) {
  settings_->SetShowContextMenuOnMouseUp(enabled);
}

void WebSettingsImpl::SetEditingBehavior(
    mojom::blink::EditingBehavior behavior) {
  settings_->SetEditingBehaviorType(behavior);
}

void WebSettingsImpl::SetHideScrollbars(bool enabled) {
  dev_tools_emulator_->SetHideScrollbars(enabled);
}

void WebSettingsImpl::SetPrefersDefaultScrollbarStyles(bool enabled) {
  settings_->SetPrefersDefaultScrollbarStyles(enabled);
}

void WebSettingsImpl::SetMockGestureTapHighlightsEnabled(bool enabled) {
  settings_->SetMockGestureTapHighlightsEnabled(enabled);
}

void WebSettingsImpl::SetAccelerated2dCanvasMSAASampleCount(int count) {
  settings_->SetAccelerated2dCanvasMSAASampleCount(count);
}

void WebSettingsImpl::SetAntialiased2dCanvasEnabled(bool enabled) {
  settings_->SetAntialiased2dCanvasEnabled(enabled);
}

void WebSettingsImpl::SetAntialiasedClips2dCanvasEnabled(bool enabled) {
  settings_->SetAntialiasedClips2dCanvasEnabled(enabled);
}

void WebSettingsImpl::SetLCDTextPreference(LCDTextPreference preference) {
  dev_tools_emulator_->SetLCDTextPreference(preference);
}

void WebSettingsImpl::SetHideDownloadUI(bool hide) {
  settings_->SetHideDownloadUI(hide);
}

void WebSettingsImpl::SetPresentationReceiver(bool enabled) {
  settings_->SetPresentationReceiver(enabled);
}

void WebSettingsImpl::SetHighlightAds(bool enabled) {
  settings_->SetHighlightAds(enabled);
}

void WebSettingsImpl::SetHyperlinkAuditingEnabled(bool enabled) {
  settings_->SetHyperlinkAuditingEnabled(enabled);
}

void WebSettingsImpl::SetValidationMessageTimerMagnification(int new_value) {
  settings_->SetValidationMessageTimerMagnification(new_value);
}

void WebSettingsImpl::SetAllowRunningOfInsecureContent(bool enabled) {
  settings_->SetAllowRunningOfInsecureContent(enabled);
}

void WebSettingsImpl::SetDisableReadingFromCanvas(bool enabled) {
  settings_->SetDisableReadingFromCanvas(enabled);
}

void WebSettingsImpl::SetStrictMixedContentChecking(bool enabled) {
  settings_->SetStrictMixedContentChecking(enabled);
}

void WebSettingsImpl::SetStrictMixedContentCheckingForPlugin(bool enabled) {
  settings_->SetStrictMixedContentCheckingForPlugin(enabled);
}

void WebSettingsImpl::SetStrictPowerfulFeatureRestrictions(bool enabled) {
  settings_->SetStrictPowerfulFeatureRestrictions(enabled);
}

void WebSettingsImpl::SetStrictlyBlockBlockableMixedContent(bool enabled) {
  settings_->SetStrictlyBlockBlockableMixedContent(enabled);
}

void WebSettingsImpl::SetPasswordEchoEnabled(bool flag) {
  settings_->SetPasswordEchoEnabled(flag);
}

void WebSettingsImpl::SetPasswordEchoDurationInSeconds(
    double duration_in_seconds) {
  settings_->SetPasswordEchoDurationInSeconds(duration_in_seconds);
}

void WebSettingsImpl::SetShouldPrintBackgrounds(bool enabled) {
  settings_->SetShouldPrintBackgrounds(enabled);
}

void WebSettingsImpl::SetShouldClearDocumentBackground(bool enabled) {
  settings_->SetShouldClearDocumentBackground(enabled);
}

void WebSettingsImpl::SetEnableScrollAnimator(bool enabled) {
  settings_->SetScrollAnimatorEnabled(enabled);
}

void WebSettingsImpl::SetPrefersReducedMotion(bool enabled) {
  settings_->SetPrefersReducedMotion(enabled);
}

void WebSettingsImpl::SetPrefersReducedTransparency(bool enabled) {
  settings_->SetPrefersReducedTransparency(enabled);
}

void WebSettingsImpl::SetInvertedColors(bool enabled) {
  settings_->SetInvertedColors(enabled);
}

bool WebSettingsImpl::ViewportEnabled() const {
  return settings_->GetViewportEnabled();
}

bool WebSettingsImpl::ViewportMetaEnabled() const {
  return settings_->GetViewportMetaEnabled();
}

bool WebSettingsImpl::DoubleTapToZoomEnabled() const {
  return dev_tools_emulator_->DoubleTapToZoomEnabled();
}

bool WebSettingsImpl::MockGestureTapHighlightsEnabled() const {
  return settings_->GetMockGestureTapHighlightsEnabled();
}

bool WebSettingsImpl::ShrinksViewportContentToFit() const {
  return settings_->GetShrinksViewportContentToFit();
}

void WebSettingsImpl::SetPictureInPictureEnabled(bool enabled) {
  settings_->SetPictureInPictureEnabled(enabled);
}

void WebSettingsImpl::SetWebAppScope(const WebString& scope) {
  settings_->SetWebAppScope(scope);
}

void WebSettingsImpl::SetPresentationRequiresUserGesture(bool required) {
  settings_->SetPresentationRequiresUserGesture(required);
}

void WebSettingsImpl::SetEmbeddedMediaExperienceEnabled(bool enabled) {
  settings_->SetEmbeddedMediaExperienceEnabled(enabled);
}

void WebSettingsImpl::SetImmersiveModeEnabled(bool enabled) {
  settings_->SetImmersiveModeEnabled(enabled);
}

void WebSettingsImpl::SetViewportEnabled(bool enabled) {
  dev_tools_emulator_->SetViewportEnabled(enabled);
}

void WebSettingsImpl::SetViewportMetaEnabled(bool enabled) {
  dev_tools_emulator_->SetViewportMetaEnabled(enabled);
}

void WebSettingsImpl::SetSyncXHRInDocumentsEnabled(bool enabled) {
  settings_->SetSyncXHRInDocumentsEnabled(enabled);
}

void WebSettingsImpl::SetTargetBlankImpliesNoOpenerEnabledWillBeRemoved(
    bool enabled) {
  settings_->SetTargetBlankImpliesNoOpenerEnabledWillBeRemoved(enabled);
}

void WebSettingsImpl::SetAllowNonEmptyNavigatorPlugins(bool enabled) {
  settings_->SetAllowNonEmptyNavigatorPlugins(enabled);
}

void WebSettingsImpl::SetCaretBrowsingEnabled(bool enabled) {
  settings_->SetCaretBrowsingEnabled(enabled);
}

void WebSettingsImpl::SetCookieEnabled(bool enabled) {
  dev_tools_emulator_->SetCookieEnabled(enabled);
}

void WebSettingsImpl::SetAllowCustomScrollbarInMainFrame(bool enabled) {
  settings_->SetAllowCustomScrollbarInMainFrame(enabled);
}

void WebSettingsImpl::SetSelectTrailingWhitespaceEnabled(bool enabled) {
  settings_->SetSelectTrailingWhitespaceEnabled(enabled);
}

void WebSettingsImpl::SetSelectionIncludesAltImageText(bool enabled) {
  settings_->SetSelectionIncludesAltImageText(enabled);
}

void WebSettingsImpl::SetSelectionStrategy(SelectionStrategyType strategy) {
  settings_->SetSelectionStrategy(static_cast<SelectionStrategy>(strategy));
}

void WebSettingsImpl::SetSmartInsertDeleteEnabled(bool enabled) {
  settings_->SetSmartInsertDeleteEnabled(enabled);
}

void WebSettingsImpl::SetMainFrameResizesAreOrientationChanges(bool enabled) {
  dev_tools_emulator_->SetMainFrameResizesAreOrientationChanges(enabled);
}

void WebSettingsImpl::SetV8CacheOptions(mojom::blink::V8CacheOptions options) {
  settings_->SetV8CacheOptions(options);
}

void WebSettingsImpl::SetViewportStyle(mojom::blink::ViewportStyle style) {
  dev_tools_emulator_->SetViewportStyle(style);
}

void WebSettingsImpl::SetMediaControlsEnabled(bool enabled) {
  settings_->SetMediaControlsEnabled(enabled);
}

void WebSettingsImpl::SetDoNotUpdateSelectionOnMutatingSelectionRange(
    bool enabled) {
  settings_->SetDoNotUpdateSelectionOnMutatingSelectionRange(enabled);
}

void WebSettingsImpl::SetLowPriorityIframesThreshold(
    WebEffectiveConnectionType effective_connection_type) {
  settings_->SetLowPriorityIframesThreshold(effective_connection_type);
}

void WebSettingsImpl::SetLazyLoadEnabled(bool enabled) {
  settings_->SetLazyLoadEnabled(enabled);
}

void WebSettingsImpl::SetLazyLoadingFrameMarginPxUnknown(int distance_px) {
  settings_->SetLazyLoadingFrameMarginPxUnknown(distance_px);
}

void WebSettingsImpl::SetLazyLoadingFrameMarginPxOffline(int distance_px) {
  settings_->SetLazyLoadingFrameMarginPxOffline(distance_px);
}

void WebSettingsImpl::SetLazyLoadingFrameMarginPxSlow2G(int distance_px) {
  settings_->SetLazyLoadingFrameMarginPxSlow2G(distance_px);
}

void WebSettingsImpl::SetLazyLoadingFrameMarginPx2G(int distance_px) {
  settings_->SetLazyLoadingFrameMarginPx2G(distance_px);
}

void WebSettingsImpl::SetLazyLoadingFrameMarginPx3G(int distance_px) {
  settings_->SetLazyLoadingFrameMarginPx3G(distance_px);
}

void WebSettingsImpl::SetLazyLoadingFrameMarginPx4G(int distance_px) {
  settings_->SetLazyLoadingFrameMarginPx4G(distance_px);
}

void WebSettingsImpl::SetLazyLoadingImageMarginPxUnknown(int distance_px) {
  settings_->SetLazyLoadingImageMarginPxUnknown(distance_px);
}

void WebSettingsImpl::SetLazyLoadingImageMarginPxOffline(int distance_px) {
  settings_->SetLazyLoadingImageMarginPxOffline(distance_px);
}

void WebSettingsImpl::SetLazyLoadingImageMarginPxSlow2G(int distance_px) {
  settings_->SetLazyLoadingImageMarginPxSlow2G(distance_px);
}

void WebSettingsImpl::SetLazyLoadingImageMarginPx2G(int distance_px) {
  settings_->SetLazyLoadingImageMarginPx2G(distance_px);
}

void WebSettingsImpl::SetLazyLoadingImageMarginPx3G(int distance_px) {
  settings_->SetLazyLoadingImageMarginPx3G(distance_px);
}

void WebSettingsImpl::SetLazyLoadingImageMarginPx4G(int distance_px) {
  settings_->SetLazyLoadingImageMarginPx4G(distance_px);
}

void WebSettingsImpl::SetForceDarkModeEnabled(bool enabled) {
  settings_->SetForceDarkModeEnabled(enabled);
}

void WebSettingsImpl::SetInForcedColors(bool in_forced_colors) {
  settings_->SetInForcedColors(in_forced_colors);
}

void WebSettingsImpl::SetIsForcedColorsDisabled(
    bool is_forced_colors_disabled) {
  settings_->SetIsForcedColorsDisabled(is_forced_colors_disabled);
}

void WebSettingsImpl::SetPreferredRootScrollbarColorScheme(
    mojom::blink::PreferredColorScheme color_scheme) {
  settings_->SetPreferredRootScrollbarColorScheme(color_scheme);
}

void WebSettingsImpl::SetPreferredColorScheme(
    mojom::blink::PreferredColorScheme color_scheme) {
  settings_->SetPreferredColorScheme(color_scheme);
}

void WebSettingsImpl::SetPreferredContrast(
    mojom::blink::PreferredContrast contrast) {
  settings_->SetPreferredContrast(contrast);
}

void WebSettingsImpl::SetNavigationControls(
    NavigationControls navigation_controls) {
  settings_->SetNavigationControls(navigation_controls);
}

void WebSettingsImpl::SetAriaModalPrunesAXTree(bool enabled) {
  settings_->SetAriaModalPrunesAXTree(enabled);
}

void WebSettingsImpl::SetSelectionClipboardBufferAvailable(bool available) {
  settings_->SetSelectionClipboardBufferAvailable(available);
}

void WebSettingsImpl::SetAccessibilityIncludeSvgGElement(bool include) {
  settings_->SetAccessibilityIncludeSvgGElement(include);
}

void WebSettingsImpl::SetWebXRImmersiveArAllowed(
    bool webxr_immersive_ar_allowed) {
  settings_->SetWebXRImmersiveArAllowed(webxr_immersive_ar_allowed);
}

void WebSettingsImpl::SetModalContextMenu(bool is_available) {
  settings_->SetModalContextMenu(is_available);
}

void WebSettingsImpl::
    SetRequireTransientActivationAndAuthorizationForSubAppsAPIs(
        bool is_required) {
  settings_->SetRequireTransientActivationAndAuthorizationForSubAppsAPI(
      is_required);
}

}  // namespace blink

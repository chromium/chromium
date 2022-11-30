// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_resource_loader.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/modules/media_controls/resources/grit/media_controls_resources.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace {

bool ShouldLoadAndroidCSS() {
#if BUILDFLAG(IS_ANDROID)
  return true;
#else
  return blink::RuntimeEnabledFeatures::MobileLayoutThemeEnabled();
#endif
}

}  // namespace

namespace blink {

MediaControlsResourceLoader::MediaControlsResourceLoader()
    : UAStyleSheetLoader() {}

MediaControlsResourceLoader::~MediaControlsResourceLoader() = default;

String MediaControlsResourceLoader::GetMediaControlsCSS() const {
  return UncompressResourceAsString(IDR_UASTYLE_MEDIA_CONTROLS_CSS);
}

String MediaControlsResourceLoader::GetMediaControlsAndroidCSS() const {
  return UncompressResourceAsString(IDR_UASTYLE_MEDIA_CONTROLS_ANDROID_CSS);
}

// static
String MediaControlsResourceLoader::GetShadowLoadingStyleSheet() {
  return UncompressResourceAsString(IDR_SHADOWSTYLE_MEDIA_CONTROLS_LOADING_CSS);
}

// static
String MediaControlsResourceLoader::GetJumpSVGImage() {
  return UncompressResourceAsString(IDR_MEDIA_CONTROLS_JUMP_SVG);
}

// static
String MediaControlsResourceLoader::GetArrowRightSVGImage() {
  return UncompressResourceAsString(IDR_MEDIA_CONTROLS_ARROW_RIGHT_SVG);
}

// static
String MediaControlsResourceLoader::GetArrowLeftSVGImage() {
  return UncompressResourceAsString(IDR_MEDIA_CONTROLS_ARROW_LEFT_SVG);
}

// static
String MediaControlsResourceLoader::GetScrubbingMessageStyleSheet() {
  return UncompressResourceAsString(
      IDR_SHADOWSTYLE_MEDIA_CONTROLS_SCRUBBING_MESSAGE_CSS);
}

// static
String MediaControlsResourceLoader::GetAnimatedArrowStyleSheet() {
  return UncompressResourceAsString(
      IDR_SHADOWSTYLE_MEDIA_CONTROLS_ANIMATED_ARROW_CSS);
}

// static
String MediaControlsResourceLoader::GetMediaInterstitialsStyleSheet() {
  return UncompressResourceAsString(IDR_UASTYLE_MEDIA_INTERSTITIALS_CSS);
}

String MediaControlsResourceLoader::GetUAStyleSheet() {
  if (ShouldLoadAndroidCSS()) {
    return GetMediaControlsCSS() + GetMediaControlsAndroidCSS() +
           GetMediaInterstitialsStyleSheet();
  }
  return GetMediaControlsCSS() + GetMediaInterstitialsStyleSheet();
}

void MediaControlsResourceLoader::InjectMediaControlsUAStyleSheet() {
  CSSDefaultStyleSheets& default_style_sheets =
      CSSDefaultStyleSheets::Instance();
  std::unique_ptr<MediaControlsResourceLoader> loader =
      std::make_unique<MediaControlsResourceLoader>();

  if (!default_style_sheets.HasMediaControlsStyleSheetLoader())
    default_style_sheets.SetMediaControlsStyleSheetLoader(std::move(loader));
}

}  // namespace blink

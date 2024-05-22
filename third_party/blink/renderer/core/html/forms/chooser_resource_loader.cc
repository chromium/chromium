// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/chooser_resource_loader.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"

namespace blink {

Vector<char> ChooserResourceLoader::GetSuggestionPickerStyleSheet() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_SUGGESTION_PICKER_CSS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetSuggestionPickerJS() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_SUGGESTION_PICKER_JS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetPickerCommonStyleSheet() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_PICKER_COMMON_CSS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetPickerCommonJS() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_PICKER_COMMON_JS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetCalendarPickerStyleSheet() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_CALENDAR_PICKER_CSS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetCalendarPickerJS() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_CALENDAR_PICKER_JS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetMonthPickerJS() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_MONTH_PICKER_JS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetTimePickerStyleSheet() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_TIME_PICKER_CSS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetTimePickerJS() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_TIME_PICKER_JS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetDateTimeLocalPickerJS() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_DATETIMELOCAL_PICKER_JS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetColorSuggestionPickerStyleSheet() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_COLOR_SUGGESTION_PICKER_CSS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetColorSuggestionPickerJS() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_COLOR_SUGGESTION_PICKER_JS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetColorPickerStyleSheet() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_COLOR_PICKER_CSS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetColorPickerJS() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_COLOR_PICKER_JS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetColorPickerCommonJS() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_COLOR_PICKER_COMMON_JS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetListPickerStyleSheet() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_LIST_PICKER_CSS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetListPickerJS() {
#if !BUILDFLAG(IS_ANDROID)
  return UncompressResourceAsBinary(IDR_LIST_PICKER_JS);
#else
  NOTREACHED_IN_MIGRATION();
  return Vector<char>();
#endif
}

}  // namespace blink

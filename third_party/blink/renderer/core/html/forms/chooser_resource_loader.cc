// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/chooser_resource_loader.h"

#include "build/build_config.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"

namespace blink {

Vector<char> ChooserResourceLoader::GetSuggestionPickerStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_SUGGESTION_PICKER_CSS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetSuggestionPickerJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_SUGGESTION_PICKER_JS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetPickerButtonStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_PICKER_BUTTON_CSS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetPickerCommonStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_PICKER_COMMON_CSS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetPickerCommonJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_PICKER_COMMON_JS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetCalendarPickerStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_CALENDAR_PICKER_CSS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetCalendarPickerJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_CALENDAR_PICKER_JS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetMonthPickerJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_MONTH_PICKER_JS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetTimePickerStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_TIME_PICKER_CSS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetTimePickerJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_TIME_PICKER_JS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetDateTimeLocalPickerJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_DATETIMELOCAL_PICKER_JS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetColorSuggestionPickerStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_COLOR_SUGGESTION_PICKER_CSS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetColorSuggestionPickerJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_COLOR_SUGGESTION_PICKER_JS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetColorPickerStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_COLOR_PICKER_CSS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetCalendarPickerRefreshStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_CALENDAR_PICKER_REFRESH_CSS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetColorPickerJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_COLOR_PICKER_JS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetColorPickerCommonJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_COLOR_PICKER_COMMON_JS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetListPickerStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_LIST_PICKER_CSS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

Vector<char> ChooserResourceLoader::GetListPickerJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsBinary(IDR_LIST_PICKER_JS);
#else
  NOTREACHED();
  return Vector<char>();
#endif
}

}  // namespace blink

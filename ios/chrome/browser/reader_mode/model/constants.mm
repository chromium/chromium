// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/constants.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

const char kReaderModeStateHistogram[] = "IOS.ReaderMode.State";

const char kReaderModeHeuristicResultHistogram[] =
    "IOS.ReaderMode.Heuristic.Result";

const char kReaderModeHeuristicLatencyHistogram[] =
    "IOS.ReaderMode.Heuristic.Latency";

const char kReaderModeDistillerLatencyHistogram[] =
    "IOS.ReaderMode.Distiller.Latency";

const char kReaderModeDistillerResultHistogram[] =
    "IOS.ReaderMode.Distiller.Result";

const char kReaderModeThemeCustomizationHistogram[] = "IOS.ReaderMode.Theme";

const char kReaderModeFontFamilyCustomizationHistogram[] =
    "IOS.ReaderMode.FontFamily";

const char kReaderModeFontScaleCustomizationHistogram[] =
    "IOS.ReaderMode.FontScale";

const char kReaderModeCustomizationHistogram[] = "IOS.ReaderMode.Customization";

const char kReaderModeTimeSpentHistogram[] = "IOS.ReaderMode.TimeSpent";

const char kReaderModeAccessPointHistogram[] = "IOS.ReaderMode.AccessPoint";

NSString* GetReaderModeSymbolName() {
  if (@available(iOS 18, *)) {
    return kReaderModeSymbolPostIOS18;
  } else {
    return kReaderModeSymbolPreIOS18;
  }
}

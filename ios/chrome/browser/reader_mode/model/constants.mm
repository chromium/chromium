// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/constants.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

const char kReaderModeHeuristicResultHistogram[] =
    "IOS.ReaderMode.Heuristic.Result";

const char kReaderModeHeuristicLatencyHistogram[] =
    "IOS.ReaderMode.Heuristic.Latency";

const char kReaderModeDistillerLatencyHistogram[] =
    "IOS.ReaderMode.Distiller.Latency";

const char kReaderModeAmpClassificationHistogram[] =
    "IOS.ReaderMode.Distiller.Amp";

NSString* GetReaderModeSymbolName() {
  if (@available(iOS 18, *)) {
    return kReaderModeSymbolPostIOS18;
  } else {
    return kReaderModeSymbolPreIOS18;
  }
}

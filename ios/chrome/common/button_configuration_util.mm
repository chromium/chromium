// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/button_configuration_util.h"

// TODO(crbug.com/1423432): Reenable warning by removing method when it's no
// longer needed.
void SetContentEdgeInsets(UIButton* button, UIEdgeInsets insets) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  button.contentEdgeInsets = insets;
#pragma clang diagnostic pop
}

// TODO(crbug.com/1423432): Reenable warning by removing method when it's no
// longer needed.
void SetImageEdgeInsets(UIButton* button, UIEdgeInsets insets) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  button.imageEdgeInsets = insets;
#pragma clang diagnostic pop
}

// TODO(crbug.com/1423432): Reenable warning by removing method when it's no
// longer needed.
void SetTitleEdgeInsets(UIButton* button, UIEdgeInsets insets) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  button.titleEdgeInsets = insets;
#pragma clang diagnostic pop
}

// TODO(crbug.com/1423432): Reenable warning by removing method when it's no
// longer needed.
void SetAdjustsImageWhenHighlighted(UIButton* button, bool isHighlighted) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  button.adjustsImageWhenHighlighted = isHighlighted;
#pragma clang diagnostic pop
}

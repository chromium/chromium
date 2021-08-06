// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_DEFAULT_BROWSER_INSTRUCTION_TABLE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_DEFAULT_BROWSER_INSTRUCTION_TABLE_VIEW_H_

#import "ios/chrome/browser/ui/elements/self_sizing_table_view.h"

// A rounded corner table view that height correspond to its content height. No
// separator line in the end and those lines does not fill the width.
@interface InstructionTableView : SelfSizingTableView

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_DEFAULT_BROWSER_INSTRUCTION_TABLE_VIEW_H_

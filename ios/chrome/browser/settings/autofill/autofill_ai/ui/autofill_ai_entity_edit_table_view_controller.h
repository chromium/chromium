// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_consumer.h"
#import "ios/chrome/browser/settings/autofill/ui/autofill_edit_table_view_controller.h"

@interface AutofillAIEntityEditTableViewController
    : AutofillEditTableViewController <AutofillAIEntityEditConsumer>

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_TABLE_VIEW_CONTROLLER_H_

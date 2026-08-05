// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_LEARN_MORE_UI_SEARCH_ENGINE_CHOICE_LEARN_MORE_CONSUMER_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_LEARN_MORE_UI_SEARCH_ENGINE_CHOICE_LEARN_MORE_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer protocol for SearchEngineChoiceLearnMoreViewController.
@protocol SearchEngineChoiceLearnMoreConsumer <NSObject>

// String ID for the third paragraph in the learn more dialog.
@property(nonatomic, assign) int thirdParagraphStringID;

@end

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_LEARN_MORE_UI_SEARCH_ENGINE_CHOICE_LEARN_MORE_CONSUMER_H_

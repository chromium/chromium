// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_CONSTANTS_H_

#import <UIKit/UIKit.h>

// This enum indicates the state of the current search engine view element in
// relation to having to highlight one of the list's elements as current default
// search engine.
//
// When the list is put together, the elements' state should be either:
// - `kNone` for all elements.
// - `kIsDefault` for the element corresponding to the current default search
// engine to highlight, and `kOtherIsDefault` for all the others.
enum class SearchEngineCurrentDefaultState {
  // There is no current default search engine to highlight.
  kNone,
  // This is not the current default search engine, but there is a current
  // default search engine.
  kOtherIsDefault,
  // This search engine is the current default.
  kIsDefault,
};

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_CONSTANTS_H_

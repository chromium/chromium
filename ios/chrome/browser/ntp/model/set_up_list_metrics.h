// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_METRICS_H_
#define IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_METRICS_H_

enum class SetUpListItemType;

namespace set_up_list_metrics {

// Records that the Set Up List was displayed.
void RecordDisplayed();

// Records that the Set Up List item of the given `type` was displayed.
void RecordItemDisplayed(SetUpListItemType type);

// Records that a Set Up List item was selected.
void RecordItemSelected(SetUpListItemType type);

// Records that a Set Up List item was completed.
void RecordItemCompleted(SetUpListItemType type);

// Records that all Set Up List items were completed.
void RecordAllItemsCompleted();

}  // namespace set_up_list_metrics

#endif  // IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_METRICS_H_

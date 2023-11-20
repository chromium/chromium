// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_autocomplete_event_view_controller.h"

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_match_formatter.h"
#import "ios/chrome/browser/ui/omnibox/popup/debugger/autocomplete_match_cell.h"
#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_autocomplete_event.h"

@implementation OmniboxAutocompleteEventViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = self.event.title;
  [self.tableView registerClass:[AutocompleteMatchCell class]
         forCellReuseIdentifier:kAutocompleteMatchCellReuseIdentifier];
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return self.event.matches.count;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  AutocompleteMatchCell* cell = [tableView
      dequeueReusableCellWithIdentifier:kAutocompleteMatchCellReuseIdentifier];

  AutocompleteMatchFormatter* matchFormatter =
      self.event.matches[indexPath.row];
  [cell setupWithAutocompleteMatchFormatter:matchFormatter];

  return cell;
}

@end

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

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return self.event.matchGroups.count;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return self.event.matchGroups[section].matches.count;
}

- (NSString*)tableView:(UITableView*)tableView
    titleForHeaderInSection:(NSInteger)section {
  return self.event.matchGroups[section].title;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  AutocompleteMatchCell* cell = [tableView
      dequeueReusableCellWithIdentifier:kAutocompleteMatchCellReuseIdentifier];

  AutocompleteMatchGroup* group = self.event.matchGroups[indexPath.section];

  AutocompleteMatchFormatter* matchFormatter = group.matches[indexPath.row];
  [cell setupWithAutocompleteMatchFormatter:matchFormatter
                           showProviderType:!group.title.length];

  return cell;
}

@end

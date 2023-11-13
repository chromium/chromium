// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_autocomplete_event_view_controller.h"

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_match_formatter.h"
#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_autocomplete_event.h"

@implementation OmniboxAutocompleteEventViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = self.event.title;
  [self.tableView registerClass:[UITableViewCell class]
         forCellReuseIdentifier:NSStringFromClass([UITableViewCell class])];
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return self.event.matches.count;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [tableView
      dequeueReusableCellWithIdentifier:NSStringFromClass(
                                            [UITableViewCell class])];
  UIListContentConfiguration* config = cell.defaultContentConfiguration;

  AutocompleteMatchFormatter* matcher = self.event.matches[indexPath.row];

  config.attributedText = matcher.text;
  cell.contentConfiguration = config;

  return cell;
}

@end

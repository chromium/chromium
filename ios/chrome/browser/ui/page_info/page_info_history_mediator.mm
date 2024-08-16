// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_history_mediator.h"

#import "components/page_info/core/page_info_history_data_source.h"

@implementation PageInfoHistoryMediator {
  std::unique_ptr<page_info::PageInfoHistoryDataSource> _historyDataSource;
}

- (instancetype)initWithHistoryService:(history::HistoryService*)historyService
                               siteURL:(GURL)siteURL {
  self = [super init];
  if (self) {
    _historyDataSource = std::make_unique<page_info::PageInfoHistoryDataSource>(
        historyService, siteURL);
  }
  return self;
}

- (void)disconnect {
  _historyDataSource.reset();
}

#pragma mark - Properties

- (void)setConsumer:(id<PageInfoHistoryConsumer>)consumer {
  _consumer = consumer;

  __weak PageInfoHistoryMediator* weakSelf = self;
  _historyDataSource->GetLastVisitedTimestamp(
      base::BindOnce(^(std::optional<base::Time> lastVisited) {
        if (lastVisited.has_value()) {
          [weakSelf.consumer setLastVisitedTimestamp:lastVisited.value()];
        }
      }));
}

@end

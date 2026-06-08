// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_breadcrumbs_view_controller.h"

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_event.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@interface AimSRPDebuggerEventCell : UITableViewCell

// The icon indicating the message direction (client-to-AIM or AIM-to-client).
@property(nonatomic, readonly) UIImageView* directionIconView;
// A header label displaying the event type.
@property(nonatomic, readonly) UILabel* headerLabel;
// The timestamp of when the event occurred.
@property(nonatomic, readonly) UILabel* timestampLabel;
// The name of the logged event/message.
@property(nonatomic, readonly) UILabel* eventNameLabel;
// The payload content of the message.
@property(nonatomic, readonly) UILabel* payloadLabel;

@end

@implementation AimSRPDebuggerEventCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.selectionStyle = UITableViewCellSelectionStyleNone;

    _directionIconView = [[UIImageView alloc] init];
    _directionIconView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_directionIconView];

    _headerLabel = [[UILabel alloc] init];
    _headerLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _headerLabel.font = [UIFont systemFontOfSize:11 weight:UIFontWeightBold];
    _headerLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _headerLabel.text = @"EVENT";
    [self.contentView addSubview:_headerLabel];

    _timestampLabel = [[UILabel alloc] init];
    _timestampLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _timestampLabel.font = [UIFont systemFontOfSize:11
                                             weight:UIFontWeightLight];
    _timestampLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    [self.contentView addSubview:_timestampLabel];

    _eventNameLabel = [[UILabel alloc] init];
    _eventNameLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _eventNameLabel.font = [UIFont systemFontOfSize:16
                                             weight:UIFontWeightSemibold];
    _eventNameLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _eventNameLabel.numberOfLines = 0;
    [self.contentView addSubview:_eventNameLabel];

    _payloadLabel = [[UILabel alloc] init];
    _payloadLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _payloadLabel.font = [UIFont systemFontOfSize:13
                                           weight:UIFontWeightRegular];
    _payloadLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _payloadLabel.numberOfLines = 0;
    [self.contentView addSubview:_payloadLabel];

    [NSLayoutConstraint activateConstraints:@[
      [_directionIconView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:16],
      [_directionIconView.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:12],
      [_directionIconView.widthAnchor constraintEqualToConstant:22],
      [_directionIconView.heightAnchor constraintEqualToConstant:22],

      [_headerLabel.leadingAnchor
          constraintEqualToAnchor:_directionIconView.trailingAnchor
                         constant:12],
      [_headerLabel.centerYAnchor
          constraintEqualToAnchor:_directionIconView.centerYAnchor],

      [_timestampLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-16],
      [_timestampLabel.centerYAnchor
          constraintEqualToAnchor:_headerLabel.centerYAnchor],

      [_eventNameLabel.leadingAnchor
          constraintEqualToAnchor:_headerLabel.leadingAnchor],
      [_eventNameLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-16],
      [_eventNameLabel.topAnchor
          constraintEqualToAnchor:_directionIconView.bottomAnchor
                         constant:6],

      [_payloadLabel.leadingAnchor
          constraintEqualToAnchor:_headerLabel.leadingAnchor],
      [_payloadLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-16],
      [_payloadLabel.topAnchor
          constraintEqualToAnchor:_eventNameLabel.bottomAnchor
                         constant:6],
      [_payloadLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-12],
    ]];
  }
  return self;
}

@end

@interface AimSRPDebuggerBreadcrumbsViewController () <UITableViewDataSource>
@end

@implementation AimSRPDebuggerBreadcrumbsViewController {
  NSArray<AimSRPDebuggerEvent*>* _breadcrumbs;
}

- (instancetype)initWithEvents:(NSArray<AimSRPDebuggerEvent*>*)events {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _breadcrumbs = [[events reverseObjectEnumerator] allObjects];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.dataSource = self;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = 100;
  self.title = @"AIM SRP Message Logs";

  UIBarButtonItem* closeButton =
      [[UIBarButtonItem alloc] initWithTitle:@"Close"
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(dismissModal)];
  self.navigationItem.rightBarButtonItem = closeButton;
}

- (void)dismissModal {
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return _breadcrumbs.count;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  AimSRPDebuggerEventCell* cell =
      [tableView dequeueReusableCellWithIdentifier:@"EventCell"];
  if (!cell) {
    cell = [[AimSRPDebuggerEventCell alloc]
          initWithStyle:UITableViewCellStyleDefault
        reuseIdentifier:@"EventCell"];
  }

  AimSRPDebuggerEvent* event = _breadcrumbs[indexPath.row];

  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:22
                          weight:UIImageSymbolWeightMedium];
  UIImage* symbol = nil;

  if (event.direction == kClientToAim) {
    symbol =
        DefaultSymbolWithConfiguration(@"arrow.up.circle.fill", symbolConfig);
    cell.directionIconView.tintColor = [UIColor colorNamed:kBlueColor];
  } else {
    symbol =
        DefaultSymbolWithConfiguration(@"arrow.down.circle.fill", symbolConfig);
    cell.directionIconView.tintColor = [UIColor colorNamed:kGreenColor];
  }

  static NSDateFormatter* formatter = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    formatter = [[NSDateFormatter alloc] init];
    [formatter setDateFormat:@"HH:mm:ss.SSS"];
  });
  NSString* timestampStr = [formatter stringFromDate:event.timestamp];

  cell.directionIconView.image = symbol;
  cell.timestampLabel.text = timestampStr;
  cell.eventNameLabel.text = event.messageName;
  cell.payloadLabel.text = event.messagePayload;

  return cell;
}

@end

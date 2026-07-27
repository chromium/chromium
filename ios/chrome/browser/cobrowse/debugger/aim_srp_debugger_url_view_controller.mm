// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_url_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_full_url_cell.h"
#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_url_component_cell.h"
#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_url_param_cell.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_ui_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "url/gurl.h"

#pragma mark - AIMSRPDebuggerURLViewController

@interface AIMSRPDebuggerURLViewController () <UITableViewDataSource,
                                               UITableViewDelegate,
                                               UITextFieldDelegate,
                                               UITextViewDelegate>
@end

@implementation AIMSRPDebuggerURLViewController {
  GURL _originalURL;
  NSURLComponents* _components;
  NSMutableArray<NSURLQueryItem*>* _queryItems;

  UITableView* _tableView;
  UIBarButtonItem* _editButton;
  UIBarButtonItem* _doneButton;
  UIBarButtonItem* _cancelButton;
  UIBarButtonItem* _closeButton;
}

- (instancetype)initWithURL:(const GURL&)url {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _originalURL = url;
    [self loadComponentsFromURL:url];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Loaded AIM URL";
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.view.accessibilityIdentifier =
      kAIMSRPDebuggerURLViewControllerAccessibilityIdentifier;

  // Set up navigation buttons.
  _closeButton =
      [[UIBarButtonItem alloc] initWithTitle:@"Close"
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(dismissModal)];
  _closeButton.accessibilityIdentifier =
      kAIMSRPDebuggerURLViewControllerCloseButtonAccessibilityIdentifier;
  self.navigationItem.leftBarButtonItem = _closeButton;

  _editButton = [[UIBarButtonItem alloc] initWithTitle:@"Edit"
                                                 style:UIBarButtonItemStylePlain
                                                target:self
                                                action:@selector(enterEditing)];
  _editButton.accessibilityIdentifier =
      kAIMSRPDebuggerURLViewControllerEditButtonAccessibilityIdentifier;
  self.navigationItem.rightBarButtonItem = _editButton;

  _cancelButton =
      [[UIBarButtonItem alloc] initWithTitle:@"Cancel"
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(cancelEditing)];

  _doneButton = [[UIBarButtonItem alloc] initWithTitle:@"Done"
                                                 style:UIBarButtonItemStyleDone
                                                target:self
                                                action:@selector(saveAndExit)];
  _doneButton.accessibilityIdentifier =
      kAIMSRPDebuggerURLViewControllerDoneButtonAccessibilityIdentifier;

  // Set up table view.
  _tableView = [[UITableView alloc] initWithFrame:self.view.bounds
                                            style:UITableViewStyleGrouped];
  _tableView.translatesAutoresizingMaskIntoConstraints = NO;
  _tableView.dataSource = self;
  _tableView.delegate = self;
  _tableView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  _tableView.separatorColor = [UIColor colorNamed:kSeparatorColor];
  _tableView.rowHeight = UITableViewAutomaticDimension;
  _tableView.estimatedRowHeight = 44.0;

  // Register cell classes.
  [_tableView registerClass:[AIMSRPDebuggerFullURLCell class]
      forCellReuseIdentifier:@"FullURLCell"];
  [_tableView registerClass:[AIMSRPDebuggerURLComponentCell class]
      forCellReuseIdentifier:@"ComponentCell"];
  [_tableView registerClass:[AIMSRPDebuggerURLParamCell class]
      forCellReuseIdentifier:@"ParamCell"];
  [_tableView registerClass:[UITableViewCell class]
      forCellReuseIdentifier:@"AddParamCell"];

  [self.view addSubview:_tableView];

  AddSameConstraints(_tableView, self.view);
}

#pragma mark - Actions

- (void)dismissModal {
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)enterEditing {
  [self setEditing:YES animated:YES];
}

- (void)cancelEditing {
  // Restore original state.
  [self loadComponentsFromURL:_originalURL];
  [self setEditing:NO animated:YES];
}

- (void)saveAndExit {
  // Reassemble URL and validate.
  _components.queryItems = _queryItems.count > 0 ? _queryItems : nil;
  NSURL* nsurl = _components.URL;
  GURL gurl = GURL(base::SysNSStringToUTF8(nsurl.absoluteString));

  if (!gurl.is_valid()) {
    UIAlertController* alert = [UIAlertController
        alertControllerWithTitle:@"Invalid URL"
                         message:@"The edited URL is not valid. Please check "
                                 @"and correct it."
                  preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                              style:UIAlertActionStyleDefault
                                            handler:nil]];
    [self presentViewController:alert animated:YES completion:nil];
    return;
  }

  [self.delegate debuggerURLViewController:self didUpdateURL:gurl];
  [self dismissModal];
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  [super setEditing:editing animated:animated];
  [_tableView setEditing:editing animated:animated];

  if (!editing) {
    [self.view endEditing:YES];
  }

  if (editing) {
    self.navigationItem.leftBarButtonItem = _cancelButton;
    self.navigationItem.rightBarButtonItem = _doneButton;
  } else {
    self.navigationItem.leftBarButtonItem = _closeButton;
    self.navigationItem.rightBarButtonItem = _editButton;
  }

  [_tableView reloadData];
}

- (void)loadComponentsFromURL:(const GURL&)url {
  NSString* urlString = base::SysUTF8ToNSString(url.spec());
  NSURL* nsurl = [NSURL URLWithString:urlString];
  _components = [NSURLComponents componentsWithURL:nsurl
                           resolvingAgainstBaseURL:NO];
  if (!_components) {
    _components = [[NSURLComponents alloc] init];
  }
  _queryItems = [_components.queryItems mutableCopy] ?: [NSMutableArray array];
}

- (NSString*)originString {
  if (!_components.scheme || !_components.host) {
    return @"";
  }
  NSString* originString = [NSString
      stringWithFormat:@"%@://%@", _components.scheme, _components.host];
  if (_components.port) {
    originString =
        [NSString stringWithFormat:@"%@:%@", originString, _components.port];
  }
  return originString;
}

#pragma mark - Field Update Helpers

- (void)updateFullURLField {
  NSIndexPath* fullURLIndexPath = [NSIndexPath indexPathForRow:0 inSection:0];
  AIMSRPDebuggerFullURLCell* cell =
      [_tableView cellForRowAtIndexPath:fullURLIndexPath];
  if (cell) {
    cell.textView.text = _components.string;
  }
}

- (void)updateComponentFieldsExceptFullURL {
  // Update Origin.
  NSIndexPath* originPath = [NSIndexPath indexPathForRow:0 inSection:1];
  AIMSRPDebuggerURLComponentCell* originCell =
      [_tableView cellForRowAtIndexPath:originPath];
  if (originCell) {
    originCell.textView.text = [self originString];
  }

  // Update Path.
  NSIndexPath* pathPath = [NSIndexPath indexPathForRow:1 inSection:1];
  AIMSRPDebuggerURLComponentCell* pathCell =
      [_tableView cellForRowAtIndexPath:pathPath];
  if (pathCell) {
    pathCell.textView.text = _components.path ?: @"";
  }

  // Reload parameters.
  [_tableView reloadSections:[NSIndexSet indexSetWithIndex:2]
            withRowAnimation:UITableViewRowAnimationNone];
}

- (void)paramFieldChanged:(UITextField*)sender {
  NSIndexPath* indexPath = [self indexPathForView:sender];
  if (!indexPath || indexPath.section != 2 ||
      indexPath.row >= static_cast<NSInteger>(_queryItems.count)) {
    return;
  }

  AIMSRPDebuggerURLParamCell* cell =
      [_tableView cellForRowAtIndexPath:indexPath];
  if (cell) {
    NSString* key = cell.keyField.text ?: @"";
    NSString* value = cell.valueField.text ?: @"";
    _queryItems[indexPath.row] = [NSURLQueryItem queryItemWithName:key
                                                             value:value];
    _components.queryItems = _queryItems;
    [self updateFullURLField];
  }
}

- (NSIndexPath*)indexPathForView:(UIView*)view {
  CGPoint point = [view convertPoint:CGPointZero toView:_tableView];
  return [_tableView indexPathForRowAtPoint:point];
}

#pragma mark - Copy Helpers

- (UIButton*)createCopyButton {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  UIImage* copyImage = SymbolWithPointSize(SymbolCopyAction, 18);
  [button setImage:copyImage forState:UIControlStateNormal];
  button.tintColor = [UIColor colorNamed:kBlueColor];
  [button addTarget:self
                action:@selector(copyButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.frame = CGRectMake(0, 0, 30, 30);
  return button;
}

- (void)copyButtonTapped:(UIButton*)sender {
  CGPoint point = [sender convertPoint:CGPointZero toView:_tableView];
  NSIndexPath* indexPath = [_tableView indexPathForRowAtPoint:point];
  if (!indexPath) {
    return;
  }

  NSString* textToCopy = nil;
  if (indexPath.section == 0) {
    textToCopy = _components.string;
  } else if (indexPath.section == 1) {
    if (indexPath.row == 0) {
      textToCopy = [self originString];
    } else if (indexPath.row == 1) {
      textToCopy = _components.path;
    }
  } else if (indexPath.section == 2) {
    if (indexPath.row < static_cast<NSInteger>(_queryItems.count)) {
      textToCopy = _queryItems[indexPath.row].value;
    }
  }

  if (textToCopy) {
    [self copyText:textToCopy fromButton:sender];
  }
}

- (void)copyText:(NSString*)text fromButton:(UIButton*)button {
  UIPasteboard.generalPasteboard.string = text;
  UIImage* checkmarkImage = SymbolWithPointSize(SymbolCheckmark, 18);
  [button setImage:checkmarkImage forState:UIControlStateNormal];
  button.enabled = NO;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.5 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        UIImage* copyImage = SymbolWithPointSize(SymbolCopyAction, 18);
        [button setImage:copyImage forState:UIControlStateNormal];
        button.enabled = YES;
      });
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 3;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  if (section == 0) {
    return 1;
  } else if (section == 1) {
    return 2;
  } else if (section == 2) {
    return _queryItems.count + (self.editing ? 1 : 0);
  }
  return 0;
}

- (NSString*)tableView:(UITableView*)tableView
    titleForHeaderInSection:(NSInteger)section {
  if (section == 0) {
    return @"Full URL";
  } else if (section == 1) {
    return @"Components";
  } else if (section == 2) {
    return @"Query Parameters";
  }
  return nil;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == 0) {
    AIMSRPDebuggerFullURLCell* cell =
        [tableView dequeueReusableCellWithIdentifier:@"FullURLCell"
                                        forIndexPath:indexPath];
    cell.textView.text = _components.string;
    cell.textView.delegate = self;
    cell.textView.editable = self.editing;
    cell.textView.textColor = self.editing
                                  ? [UIColor colorNamed:kTextPrimaryColor]
                                  : [UIColor colorNamed:kTextSecondaryColor];
    cell.textView.accessibilityIdentifier =
        kAIMSRPDebuggerURLViewControllerTextViewAccessibilityIdentifier;

    if (self.editing) {
      cell.accessoryView = nil;
    } else {
      cell.accessoryView = [self createCopyButton];
    }
    return cell;
  } else if (indexPath.section == 1) {
    AIMSRPDebuggerURLComponentCell* cell =
        [tableView dequeueReusableCellWithIdentifier:@"ComponentCell"
                                        forIndexPath:indexPath];
    cell.textView.delegate = self;
    cell.textView.editable = self.editing;
    cell.textView.textColor = self.editing
                                  ? [UIColor colorNamed:kTextPrimaryColor]
                                  : [UIColor colorNamed:kTextSecondaryColor];

    if (indexPath.row == 0) {
      cell.titleLabel.text = @"Origin";
      cell.textView.text = [self originString];
    } else {
      cell.titleLabel.text = @"Path";
      cell.textView.text = _components.path ?: @"";
    }

    if (self.editing) {
      cell.accessoryView = nil;
    } else {
      cell.accessoryView = [self createCopyButton];
    }
    return cell;
  } else if (indexPath.section == 2) {
    if (indexPath.row < static_cast<NSInteger>(_queryItems.count)) {
      AIMSRPDebuggerURLParamCell* cell =
          [tableView dequeueReusableCellWithIdentifier:@"ParamCell"
                                          forIndexPath:indexPath];
      NSURLQueryItem* item = _queryItems[indexPath.row];

      cell.keyField.text = item.name;
      cell.keyField.enabled = self.editing;
      cell.keyField.textColor = self.editing
                                    ? [UIColor colorNamed:kTextPrimaryColor]
                                    : [UIColor colorNamed:kTextSecondaryColor];
      cell.keyField.delegate = self;

      [cell.keyField removeTarget:nil
                           action:NULL
                 forControlEvents:UIControlEventEditingChanged];
      [cell.keyField addTarget:self
                        action:@selector(paramFieldChanged:)
              forControlEvents:UIControlEventEditingChanged];

      cell.valueField.text = item.value;
      cell.valueField.editable = self.editing;
      cell.valueField.textColor =
          self.editing ? [UIColor colorNamed:kTextPrimaryColor]
                       : [UIColor colorNamed:kTextSecondaryColor];
      cell.valueField.delegate = self;

      if (self.editing) {
        cell.accessoryView = nil;
      } else {
        cell.accessoryView = [self createCopyButton];
      }
      return cell;
    } else {
      UITableViewCell* cell =
          [tableView dequeueReusableCellWithIdentifier:@"AddParamCell"
                                          forIndexPath:indexPath];
      UIListContentConfiguration* content = [cell defaultContentConfiguration];
      content.text = @"add parameter";
      content.textProperties.color = [UIColor colorNamed:kBlueColor];
      cell.contentConfiguration = content;
      return cell;
    }
  }
  return nil;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];

  if (indexPath.section == 2 && self.editing &&
      indexPath.row == static_cast<NSInteger>(_queryItems.count)) {
    [self tableView:tableView
        commitEditingStyle:UITableViewCellEditingStyleInsert
         forRowAtIndexPath:indexPath];
  }
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == 2) {
    return YES;
  }
  return NO;
}

- (UITableViewCellEditingStyle)tableView:(UITableView*)tableView
           editingStyleForRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == 2) {
    if (indexPath.row < static_cast<NSInteger>(_queryItems.count)) {
      return UITableViewCellEditingStyleDelete;
    } else {
      return UITableViewCellEditingStyleInsert;
    }
  }
  return UITableViewCellEditingStyleNone;
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == 2) {
    if (editingStyle == UITableViewCellEditingStyleDelete) {
      [_queryItems removeObjectAtIndex:indexPath.row];
      _components.queryItems = _queryItems.count > 0 ? _queryItems : nil;
      [self updateFullURLField];
      [tableView deleteRowsAtIndexPaths:@[ indexPath ]
                       withRowAnimation:UITableViewRowAnimationAutomatic];
      [tableView reloadSections:[NSIndexSet indexSetWithIndex:2]
               withRowAnimation:UITableViewRowAnimationNone];
    } else if (editingStyle == UITableViewCellEditingStyleInsert) {
      NSURLQueryItem* newItem = [NSURLQueryItem queryItemWithName:@""
                                                            value:@""];
      [_queryItems addObject:newItem];
      _components.queryItems = _queryItems;
      [self updateFullURLField];
      NSIndexPath* newRowPath =
          [NSIndexPath indexPathForRow:_queryItems.count - 1 inSection:2];
      [tableView insertRowsAtIndexPaths:@[ newRowPath ]
                       withRowAnimation:UITableViewRowAnimationAutomatic];
    }
  }
}

#pragma mark - UITextViewDelegate

- (void)textViewDidChange:(UITextView*)textView {
  NSIndexPath* indexPath = [self indexPathForView:textView];
  if (!indexPath) {
    return;
  }

  if (indexPath.section == 0) {
    NSString* newURLString = textView.text;
    NSURL* url = [NSURL URLWithString:newURLString];
    if (url) {
      _components = [NSURLComponents componentsWithURL:url
                               resolvingAgainstBaseURL:NO];
      _queryItems =
          [_components.queryItems mutableCopy] ?: [NSMutableArray array];
      [self updateComponentFieldsExceptFullURL];
    }
  } else if (indexPath.section == 1) {
    if (indexPath.row == 0) {
      NSURL* newOrigin = [NSURL URLWithString:textView.text];
      if (newOrigin) {
        _components.scheme = newOrigin.scheme;
        _components.host = newOrigin.host;
        _components.port = newOrigin.port;
      } else {
        _components.scheme = nil;
        _components.host = nil;
        _components.port = nil;
      }
    } else if (indexPath.row == 1) {
      _components.path = textView.text;
    }
    [self updateFullURLField];
  } else if (indexPath.section == 2) {
    if (indexPath.row < static_cast<NSInteger>(_queryItems.count)) {
      NSString* name = _queryItems[indexPath.row].name;
      _queryItems[indexPath.row] =
          [NSURLQueryItem queryItemWithName:name value:textView.text];
      _components.queryItems = _queryItems;
      [self updateFullURLField];
    }
  }

  // Force height updates to match the expanding text view without losing focus.
  [_tableView performBatchUpdates:nil completion:nil];
}

@end

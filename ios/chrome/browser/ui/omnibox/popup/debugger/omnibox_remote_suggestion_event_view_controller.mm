// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_remote_suggestion_event_view_controller.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_remote_suggestion_event.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation OmniboxRemoteSuggestionEventViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = UIColor.systemBackgroundColor;

  UIButton* copyRequestURL = [UIButton
      systemButtonWithImage:DefaultSymbolWithPointSize(kCopyActionSymbol,
                                                       kSymbolActionPointSize)
                     target:self
                     action:@selector(didTapCopyRequestURLButton)];
  copyRequestURL.translatesAutoresizingMaskIntoConstraints = NO;
  [copyRequestURL setTitle:@"Copy Request URL" forState:UIControlStateNormal];

  UIButton* copyResponseBodyButton = [UIButton
      systemButtonWithImage:DefaultSymbolWithPointSize(kCopyActionSymbol,
                                                       kSymbolActionPointSize)
                     target:self
                     action:@selector(didTapCopyResponseBodyButton)];
  copyResponseBodyButton.translatesAutoresizingMaskIntoConstraints = NO;
  [copyResponseBodyButton setTitle:@"Copy Response"
                          forState:UIControlStateNormal];

  UIStackView* copyStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ copyRequestURL, copyResponseBodyButton ]];
  copyStackView.distribution = UIStackViewDistributionFillEqually;
  copyStackView.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel* requestURLLabel = [[UILabel alloc] init];
  requestURLLabel.translatesAutoresizingMaskIntoConstraints = NO;
  requestURLLabel.text = self.event.requestURL;
  requestURLLabel.numberOfLines = 0;

  UILabel* requestBodyLabel = [[UILabel alloc] init];
  requestBodyLabel.translatesAutoresizingMaskIntoConstraints = NO;
  requestBodyLabel.text = [self prettifyJsonString:self.event.requestBody];
  requestBodyLabel.numberOfLines = 0;

  UILabel* responseLabel = [[UILabel alloc] init];
  responseLabel.translatesAutoresizingMaskIntoConstraints = NO;
  responseLabel.text = [self prettifyJsonString:self.event.responseBody];
  responseLabel.numberOfLines = 0;

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    copyStackView, requestURLLabel, requestBodyLabel, responseLabel
  ]];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisVertical;

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [scrollView addSubview:stackView];

  [self.view addSubview:scrollView];

  AddSameConstraints(self.view, scrollView);
  AddSameConstraints(stackView, scrollView);

  [NSLayoutConstraint activateConstraints:@[
    [stackView.widthAnchor constraintEqualToAnchor:scrollView.widthAnchor]
  ]];
}

- (void)didTapCopyResponseBodyButton {
  UIPasteboard.generalPasteboard.string =
      [self prettifyJsonString:self.event.responseBody];
}

- (void)didTapCopyRequestURLButton {
  UIPasteboard.generalPasteboard.string = self.event.requestURL;
}

- (NSString*)prettifyJsonString:(NSString*)jsonString {
  if (!jsonString) {
    return jsonString;
  }

  NSError* error;

  NSData* jsonData = [jsonString dataUsingEncoding:NSUTF8StringEncoding];
  if (!jsonData) {
    return jsonString;
  }

  NSDictionary* jsonObject = [NSJSONSerialization JSONObjectWithData:jsonData
                                                             options:0
                                                               error:&error];
  if (!jsonObject) {
    return jsonString;
  }
  NSData* prettyJsonData =
      [NSJSONSerialization dataWithJSONObject:jsonObject
                                      options:NSJSONWritingPrettyPrinted
                                        error:&error];
  if (!prettyJsonData) {
    return jsonString;
  }
  NSString* prettyPrintedJson =
      [[NSString alloc] initWithData:prettyJsonData
                            encoding:NSUTF8StringEncoding];
  return prettyPrintedJson;
}

@end

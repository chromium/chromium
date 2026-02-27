// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_actuation_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_mutator.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
NSString* const kToolNavigate = @"Navigate";
NSString* const kToolClick = @"Click";
}  // namespace

@interface AIPrototypingActuationViewController () <UITextViewDelegate> {
  UIButton* _toolPickerButton;
  UIButton* _submitButton;
  UIButton* _clearButton;
  UITextView* _responseContainer;
  UIButton* _tabIdButton;
  UITextView* _jsonInputView;
  UIView* _tabIdContainer;
  UIView* _jsonContainer;

  NSString* _selectedTabId;
  NSString* _activeTabId;

  // Completion handler for deferred menu element.
  void (^_menuCompletion)(NSArray<UIMenuElement*>*);

  // Configuration map for tools.
  // Keys: Tool Name (NSString)
  // Values: NSDictionary with keys:
  //   - @"ui": NSArray of UIViews to show
  //   - @"json": NSString template for JSON
  NSDictionary<NSString*, NSDictionary*>* _toolConfigs;
}

@end

@implementation AIPrototypingActuationViewController

@synthesize mutator = _mutator;
@synthesize feature = _feature;

- (instancetype)initForFeature:(AIPrototypingFeature)feature {
  self = [super init];
  if (self) {
    _feature = feature;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.sheetPresentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent],
  ];

  [self setupSubviews];
  [self setupToolConfigs];

  // Default tool
  [self selectTool:kToolNavigate];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.mutator listTabs];
}

#pragma mark - Setup

- (void)setupSubviews {
  UIColor* primaryColor = [UIColor colorNamed:kTextPrimaryColor];

  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  label.text = @"Actuation Prototype";

  _tabIdButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [_tabIdButton setTitle:@"Select Tab" forState:UIControlStateNormal];
  _tabIdButton.showsMenuAsPrimaryAction = YES;
  [self updateMenuWithDeferredElement];

  UILabel* tabLabel = [[UILabel alloc] init];
  tabLabel.text = @"Tab:";
  [tabLabel setContentHuggingPriority:UILayoutPriorityRequired
                              forAxis:UILayoutConstraintAxisHorizontal];

  UIView* borderedTabContainer = [[UIView alloc] init];
  borderedTabContainer.translatesAutoresizingMaskIntoConstraints = NO;
  borderedTabContainer.layer.borderColor = [[UIColor blackColor] CGColor];
  borderedTabContainer.layer.borderWidth = kBorderWidth;
  borderedTabContainer.layer.cornerRadius = kCornerRadius;
  [borderedTabContainer addSubview:_tabIdButton];
  _tabIdButton.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [_tabIdButton.leadingAnchor
        constraintEqualToAnchor:borderedTabContainer.leadingAnchor
                       constant:kHorizontalInset],
    [_tabIdButton.trailingAnchor
        constraintEqualToAnchor:borderedTabContainer.trailingAnchor
                       constant:-kHorizontalInset],
    [_tabIdButton.topAnchor
        constraintEqualToAnchor:borderedTabContainer.topAnchor
                       constant:kVerticalInset],
    [_tabIdButton.bottomAnchor
        constraintEqualToAnchor:borderedTabContainer.bottomAnchor
                       constant:-kVerticalInset],
  ]];

  UIStackView* tabStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ tabLabel, borderedTabContainer ]];
  tabStack.translatesAutoresizingMaskIntoConstraints = NO;
  tabStack.axis = UILayoutConstraintAxisHorizontal;
  tabStack.spacing = kButtonStackViewSpacing;

  _tabIdContainer = [[UIView alloc] init];
  _tabIdContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [_tabIdContainer addSubview:tabStack];
  _tabIdContainer.hidden = YES;

  [NSLayoutConstraint activateConstraints:@[
    [tabStack.leadingAnchor
        constraintEqualToAnchor:_tabIdContainer.leadingAnchor],
    [tabStack.trailingAnchor
        constraintEqualToAnchor:_tabIdContainer.trailingAnchor],
    [tabStack.topAnchor constraintEqualToAnchor:_tabIdContainer.topAnchor],
    [tabStack.bottomAnchor
        constraintEqualToAnchor:_tabIdContainer.bottomAnchor],
  ]];

  _jsonInputView = [[UITextView alloc] init];
  _jsonInputView.translatesAutoresizingMaskIntoConstraints = NO;
  _jsonInputView.font = [UIFont fontWithName:@"Menlo" size:12];
  _jsonInputView.layer.borderColor = [primaryColor CGColor];
  _jsonInputView.layer.borderWidth = 1.0;
  _jsonInputView.layer.cornerRadius = kCornerRadius;
  _jsonInputView.text = @"";

  _jsonContainer =
      [[UIStackView alloc] initWithArrangedSubviews:@[ _jsonInputView ]];
  ((UIStackView*)_jsonContainer).axis = UILayoutConstraintAxisHorizontal;
  ((UIStackView*)_jsonContainer).spacing = kButtonStackViewSpacing;
  _jsonContainer.hidden = YES;

  _toolPickerButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _toolPickerButton.layer.borderColor = [primaryColor CGColor];
  _toolPickerButton.layer.borderWidth = kBorderWidth;
  _toolPickerButton.layer.cornerRadius = kCornerRadius;
  [_toolPickerButton setTitle:@"Select Tool" forState:UIControlStateNormal];
  [_toolPickerButton setTitleColor:primaryColor forState:UIControlStateNormal];
  // Menu will be set in setupToolConfigs
  _toolPickerButton.showsMenuAsPrimaryAction = YES;

  UILabel* toolLabel = [[UILabel alloc] init];
  toolLabel.text = @"Tool:";
  [toolLabel setContentHuggingPriority:UILayoutPriorityRequired
                               forAxis:UILayoutConstraintAxisHorizontal];

  UIStackView* toolStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ toolLabel, _toolPickerButton ]];
  toolStack.axis = UILayoutConstraintAxisHorizontal;
  toolStack.spacing = kButtonStackViewSpacing;

  _submitButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _submitButton.backgroundColor = [UIColor colorNamed:kBlueColor];
  _submitButton.layer.cornerRadius = kCornerRadius;
  [_submitButton setTitle:@"Execute Action" forState:UIControlStateNormal];
  [_submitButton setTitleColor:primaryColor forState:UIControlStateNormal];
  [_submitButton addTarget:self
                    action:@selector(onSubmitButtonPressed:)
          forControlEvents:UIControlEventTouchUpInside];

  _clearButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [_clearButton setTitle:@"Clear Results" forState:UIControlStateNormal];
  [_clearButton setTitleColor:primaryColor forState:UIControlStateNormal];
  [_clearButton addTarget:self
                   action:@selector(onClearButtonPressed:)
         forControlEvents:UIControlEventTouchUpInside];

  UIStackView* buttonsStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _submitButton, _clearButton ]];
  buttonsStack.axis = UILayoutConstraintAxisHorizontal;
  buttonsStack.spacing = kButtonStackViewSpacing;
  buttonsStack.distribution = UIStackViewDistributionFillEqually;

  _responseContainer = [UITextView textViewUsingTextLayoutManager:NO];
  _responseContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _responseContainer.editable = NO;
  _responseContainer.layer.cornerRadius = kCornerRadius;
  _responseContainer.layer.masksToBounds = YES;
  _responseContainer.layer.borderColor = [primaryColor CGColor];
  _responseContainer.layer.borderWidth = kBorderWidth;
  _responseContainer.textContainer.lineBreakMode = NSLineBreakByWordWrapping;
  _responseContainer.text = @"Result will appear here...";

  UIStackView* mainStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    label, toolStack, _tabIdContainer, _jsonContainer, buttonsStack,
    _responseContainer
  ]];
  mainStack.translatesAutoresizingMaskIntoConstraints = NO;
  mainStack.axis = UILayoutConstraintAxisVertical;
  mainStack.spacing = kMainStackViewSpacing;
  [self.view addSubview:mainStack];

  [NSLayoutConstraint activateConstraints:@[
    [mainStack.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                        constant:kMainStackTopInset],
    [mainStack.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                            constant:kHorizontalInset],
    [mainStack.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                             constant:-kHorizontalInset],
    [_jsonInputView.heightAnchor constraintEqualToConstant:150],
    [_responseContainer.heightAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.heightAnchor
                                  multiplier:
                                      kResponseContainerHeightMultiplier],
  ]];
}

- (void)setupToolConfigs {
  _toolConfigs = @{
    kToolNavigate : @{
      @"ui" : @[ _tabIdContainer, _jsonContainer ],
      @"json" : @("{\n"
                  "  \"navigate\": {\n"
                  "    \"tab_id\": %d,\n"
                  "    \"url\": \"https://www.google.com\"\n"
                  "  }\n"
                  "}")
    },
    kToolClick : @{
      @"ui" : @[ _tabIdContainer, _jsonContainer ],
      @"json" : @("{\n"
                  "  \"click\": {\n"
                  "    \"tab_id\": %d,\n"
                  "    \"target\": {\n"
                  "      \"coordinate\": {\n"
                  "        \"x\": 200,\n"
                  "        \"y\": 200\n"
                  "      }\n"
                  "    },\n"
                  "    \"click_type\": 1,\n"
                  "    \"click_count\": 1\n"
                  "  }\n"
                  "}")
    }
  };

  _toolPickerButton.menu = [self createToolPickerMenu];
}

#pragma mark - Private

- (void)updateMenuWithDeferredElement {
  __weak __typeof(self) weakSelf = self;
  UIDeferredMenuElement* deferredElement =
      [UIDeferredMenuElement elementWithUncachedProvider:^(
                                 void (^completion)(NSArray<UIMenuElement*>*)) {
        __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        strongSelf->_menuCompletion = completion;
        [strongSelf.mutator listTabs];
      }];
  _tabIdButton.menu = [UIMenu menuWithTitle:@"Select Tab"
                                   children:@[ deferredElement ]];
}

- (void)updateJsonTemplateForTool:(NSString*)toolName {
  int tabId = [_selectedTabId intValue];
  if (tabId == 0 && _activeTabId) {
    tabId = [_activeTabId intValue];
  }

  NSDictionary* config = _toolConfigs[toolName];
  if (config) {
    NSString* templateString = config[@"json"];
    NSString* newText;
    // Assuming the template has one %d placeholder for tab_id.
    // If it doesn't, this won't crash but might not format as expected if
    // placeholders differ. Simple safety check:
    if ([templateString containsString:@"%d"]) {
      newText = [NSString stringWithFormat:templateString, tabId];
    } else {
      newText = templateString;
    }
    _jsonInputView.text = newText;
  }
}

- (void)selectTool:(NSString*)toolName {
  [_toolPickerButton setTitle:toolName forState:UIControlStateNormal];
  [self updateJsonTemplateForTool:toolName];

  _tabIdContainer.hidden = YES;
  _jsonContainer.hidden = YES;

  NSDictionary* config = _toolConfigs[toolName];
  if (config) {
    for (UIView* view in config[@"ui"]) {
      view.hidden = NO;
    }
  }
}

- (UIMenu*)createToolPickerMenu {
  NSMutableArray<UIAction*>* actions = [NSMutableArray array];
  __weak __typeof(self) weakSelf = self;
  for (NSString* toolName in _toolConfigs) {
    UIAction* action = [UIAction actionWithTitle:toolName
                                           image:nil
                                      identifier:nil
                                         handler:^(UIAction* act) {
                                           [weakSelf selectTool:toolName];
                                         }];
    [actions addObject:action];
  }
  return [UIMenu menuWithTitle:@"Select Tool with preset values"
                      children:actions];
}

- (void)onSubmitButtonPressed:(UIButton*)sender {
  [self disableSubmitButtons];
  _responseContainer.text = @"Executing...";

  NSMutableDictionary* params = [NSMutableDictionary dictionary];
  if (!_jsonContainer.hidden) {
    params[@"json"] = _jsonInputView.text;
  }
  [self.mutator executeActuationWithParams:params];
}

- (void)onClearButtonPressed:(UIButton*)sender {
  _responseContainer.text = @"";
  [self enableSubmitButtons];
}

#pragma mark - AIPrototypingViewControllerProtocol

- (void)updateTabList:(NSArray<NSDictionary*>*)tabs {
  NSMutableArray<UIAction*>* actions = [NSMutableArray array];
  __weak __typeof(self) weakSelf = self;
  __block NSString* activeTabDisplayTitle = @"Active Tab";

  for (NSDictionary* tab in tabs) {
    NSString* title = tab[@"title"];
    NSString* tabID = [tab[@"id"] stringValue];
    BOOL isActive = [tab[@"active"] boolValue];

    if (isActive) {
      _activeTabId = tabID;
    }

    NSString* displayTitle;
    if (isActive) {
      displayTitle =
          [NSString stringWithFormat:@"Active: %@ - %@", tabID, title];
      activeTabDisplayTitle = displayTitle;
    } else {
      displayTitle = [NSString stringWithFormat:@"%@ - %@", tabID, title];
    }

    UIAction* action = [UIAction
        actionWithTitle:displayTitle
                  image:nil
             identifier:nil
                handler:^(UIAction* menuAction) {
                  __typeof(self) strongSelf = weakSelf;
                  if (!strongSelf) {
                    return;
                  }
                  strongSelf->_selectedTabId = isActive ? nil : tabID;
                  [strongSelf->_tabIdButton setTitle:displayTitle
                                            forState:UIControlStateNormal];
                  [strongSelf
                      updateJsonTemplateForTool:strongSelf->_toolPickerButton
                                                    .titleLabel.text];
                }];
    [actions addObject:action];
  }

  if (_menuCompletion) {
    _menuCompletion(actions);
    _menuCompletion = nil;
  }

  if (!_selectedTabId) {
    [_tabIdButton setTitle:activeTabDisplayTitle forState:UIControlStateNormal];
  }
  [self updateJsonTemplateForTool:_toolPickerButton.titleLabel.text];
}

- (void)updateResponseField:(NSString*)response {
  _responseContainer.text = response;
  [self enableSubmitButtons];
}

- (void)enableSubmitButtons {
  _submitButton.enabled = YES;
  _submitButton.backgroundColor = [UIColor colorNamed:kBlueColor];
}

- (void)disableSubmitButtons {
  _submitButton.enabled = NO;
  _submitButton.backgroundColor = [UIColor colorNamed:kDisabledTintColor];
}

@end

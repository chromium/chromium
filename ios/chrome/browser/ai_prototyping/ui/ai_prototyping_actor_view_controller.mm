// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_actor_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_mutator.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Tool names.
NSString* const kToolNavigate = @"Navigate";
NSString* const kToolClick = @"Click";
NSString* const kToolHistoryBack = @"History Back";
NSString* const kToolHistoryForward = @"History Forward";
NSString* const kToolType = @"Type";
NSString* const kToolWait = @"Wait";
NSString* const kToolMultiTool = @"Multi-tool";
NSString* const kToolScroll = @"Scroll";
NSString* const kToolScrollTo = @"Scroll To";
NSString* const kToolSelect = @"Select";

// Placeholder macro for tab ID.
NSString* const kTabIdMacro = @"{{tab_id}}";

// Whether the tool injects custom JavaScript on the page. For debugging
// purposes, more tab related information will be exposed and available for
// modification for these tools for debugging.
bool IsWebActuationTool(NSString* tool) {
  return [tool isEqualToString:kToolClick] ||
         [tool isEqualToString:kToolType] ||
         [tool isEqualToString:kToolMultiTool] ||
         [tool isEqualToString:kToolScroll] ||
         [tool isEqualToString:kToolScrollTo] ||
         [tool isEqualToString:kToolSelect];
}
}  // namespace

@interface AIPrototypingActorViewController () <UITextViewDelegate> {
  UIButton* _toolPickerButton;
  UIButton* _submitButton;
  UIButton* _clearButton;
  UIButton* _updateAPCDebugData;
  UITextView* _responseContainer;
  UITextView* _framesAndContentNodesContainer;
  UIButton* _tabIdButton;
  UITextView* _jsonInputView;
  UIView* _tabIdContainer;
  UIButton* _frameIdButton;
  UIView* _frameIdContainer;
  UIView* _jsonContainer;

  NSString* _selectedTabId;
  NSString* _activeTabId;
  NSString* _selectedFrameId;

  // Completion handler for deferred menu element.
  void (^_menuCompletion)(NSArray<UIMenuElement*>*);
  void (^_frameMenuCompletion)(NSArray<UIMenuElement*>*);

  UILabel* _framesAndContentNodesLabel;

  // Configuration map for tools. Note that tool templates specify coordinates
  // by default for targeting (where applicable).
  // Keys: Tool Name (NSString)
  // Values: NSDictionary with keys:
  //   - @"ui": NSArray of UIViews to show
  //   - @"template": NSDictionary template for JSON
  NSDictionary<NSString*, NSDictionary*>* _toolConfigs;
}

@end

@implementation AIPrototypingActorViewController

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
  [self selectTool:kToolMultiTool];
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
  label.text = @"Actor Prototype";

  _tabIdButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [_tabIdButton setTitle:@"Select Tab" forState:UIControlStateNormal];
  _tabIdButton.showsMenuAsPrimaryAction = YES;
  [self updateMenuWithDeferredElement];

  UILabel* tabLabel = [[UILabel alloc] init];
  tabLabel.text = @"Tab:";
  tabLabel.textColor = primaryColor;
  [tabLabel setContentHuggingPriority:UILayoutPriorityRequired
                              forAxis:UILayoutConstraintAxisHorizontal];
  [tabLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
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

  _frameIdButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [_frameIdButton setTitle:@"Select Frame" forState:UIControlStateNormal];
  _frameIdButton.showsMenuAsPrimaryAction = YES;

  __weak __typeof(self) weakSelf = self;
  UIDeferredMenuElement* deferredFrameElement =
      [UIDeferredMenuElement elementWithUncachedProvider:^(
                                 void (^completion)(NSArray<UIMenuElement*>*)) {
        __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        strongSelf->_frameMenuCompletion = completion;
        [strongSelf.mutator executeAPCExtractionWithRichExtraction:YES
                                                    actionableMode:YES
                                                  includeDebugData:YES];
      }];
  _frameIdButton.menu = [UIMenu menuWithTitle:@"Select Frame"
                                     children:@[ deferredFrameElement ]];

  UILabel* frameLabel = [[UILabel alloc] init];
  frameLabel.text = @"Frame";
  frameLabel.textColor = primaryColor;
  [frameLabel setContentHuggingPriority:UILayoutPriorityRequired
                                forAxis:UILayoutConstraintAxisHorizontal];
  [frameLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  UIView* borderedFrameContainer = [[UIView alloc] init];
  borderedFrameContainer.translatesAutoresizingMaskIntoConstraints = NO;
  borderedFrameContainer.layer.borderColor = [[UIColor blackColor] CGColor];
  borderedFrameContainer.layer.borderWidth = kBorderWidth;
  borderedFrameContainer.layer.cornerRadius = kCornerRadius;
  [borderedFrameContainer addSubview:_frameIdButton];
  _frameIdButton.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [_frameIdButton.leadingAnchor
        constraintEqualToAnchor:borderedFrameContainer.leadingAnchor
                       constant:kHorizontalInset],
    [_frameIdButton.trailingAnchor
        constraintEqualToAnchor:borderedFrameContainer.trailingAnchor
                       constant:-kHorizontalInset],
    [_frameIdButton.topAnchor
        constraintEqualToAnchor:borderedFrameContainer.topAnchor
                       constant:kVerticalInset],
    [_frameIdButton.bottomAnchor
        constraintEqualToAnchor:borderedFrameContainer.bottomAnchor
                       constant:-kVerticalInset],
  ]];

  UIStackView* frameStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ frameLabel, borderedFrameContainer ]];
  frameStack.translatesAutoresizingMaskIntoConstraints = NO;
  frameStack.axis = UILayoutConstraintAxisHorizontal;
  frameStack.spacing = kButtonStackViewSpacing;

  _frameIdContainer = [[UIView alloc] init];
  _frameIdContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [_frameIdContainer addSubview:frameStack];
  _frameIdContainer.hidden = YES;

  [NSLayoutConstraint activateConstraints:@[
    [frameStack.leadingAnchor
        constraintEqualToAnchor:_frameIdContainer.leadingAnchor],
    [frameStack.trailingAnchor
        constraintEqualToAnchor:_frameIdContainer.trailingAnchor],
    [frameStack.topAnchor constraintEqualToAnchor:_frameIdContainer.topAnchor],
    [frameStack.bottomAnchor
        constraintEqualToAnchor:_frameIdContainer.bottomAnchor],
  ]];

  _jsonInputView = [[UITextView alloc] init];
  _jsonInputView.translatesAutoresizingMaskIntoConstraints = NO;
  _jsonInputView.font = [UIFont fontWithName:@"Menlo" size:12];
  _jsonInputView.layer.borderColor = [primaryColor CGColor];
  _jsonInputView.layer.borderWidth = 1.0;
  _jsonInputView.layer.cornerRadius = kCornerRadius;
  _jsonInputView.autocapitalizationType = UITextAutocapitalizationTypeNone;
  _jsonInputView.smartQuotesType = UITextSmartQuotesTypeNo;
  _jsonInputView.text = @"";

  UILabel* jsonLabel = [[UILabel alloc] init];
  jsonLabel.text = @"Editable JSON";
  jsonLabel.textColor = primaryColor;
  [jsonLabel setContentHuggingPriority:UILayoutPriorityRequired
                               forAxis:UILayoutConstraintAxisVertical];

  _jsonContainer = [[UIStackView alloc]
      initWithArrangedSubviews:@[ jsonLabel, _jsonInputView ]];
  ((UIStackView*)_jsonContainer).axis = UILayoutConstraintAxisVertical;
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
  toolLabel.textColor = primaryColor;
  [toolLabel setContentHuggingPriority:UILayoutPriorityRequired
                               forAxis:UILayoutConstraintAxisHorizontal];
  [toolLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
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

  _updateAPCDebugData = [UIButton buttonWithType:UIButtonTypeSystem];
  [_updateAPCDebugData setTitle:@"Update APC" forState:UIControlStateNormal];
  [_updateAPCDebugData setTitleColor:primaryColor
                            forState:UIControlStateNormal];
  [_updateAPCDebugData addTarget:self
                          action:@selector(onUpdateApcButtonPressed:)
                forControlEvents:UIControlEventTouchUpInside];

  UIStackView* buttonsStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _submitButton, _clearButton, _updateAPCDebugData
  ]];
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

  _framesAndContentNodesContainer =
      [UITextView textViewUsingTextLayoutManager:NO];
  _framesAndContentNodesContainer.translatesAutoresizingMaskIntoConstraints =
      NO;
  _framesAndContentNodesContainer.editable = NO;
  _framesAndContentNodesContainer.font = [UIFont fontWithName:@"Menlo" size:10];
  _framesAndContentNodesContainer.layer.cornerRadius = kCornerRadius;
  _framesAndContentNodesContainer.layer.masksToBounds = YES;
  _framesAndContentNodesContainer.layer.borderColor = [primaryColor CGColor];
  _framesAndContentNodesContainer.layer.borderWidth = kBorderWidth;
  _framesAndContentNodesContainer.text =
      @"A debug string from APC data will appear here...";

  _framesAndContentNodesLabel = [[UILabel alloc] init];
  _framesAndContentNodesLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _framesAndContentNodesLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  _framesAndContentNodesLabel.text = @"Frames and Content Nodes:";
  _framesAndContentNodesLabel.textColor = primaryColor;

  UIStackView* mainStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    label, toolStack, _tabIdContainer, _frameIdContainer, _jsonContainer,
    buttonsStack, _responseContainer, _framesAndContentNodesLabel,
    _framesAndContentNodesContainer
  ]];
  mainStack.translatesAutoresizingMaskIntoConstraints = NO;
  mainStack.axis = UILayoutConstraintAxisVertical;
  mainStack.spacing = kMainStackViewSpacing;

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:scrollView];
  [scrollView addSubview:mainStack];

  [NSLayoutConstraint activateConstraints:@[
    [scrollView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],

    [mainStack.topAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.topAnchor
                       constant:kMainStackTopInset],
    [mainStack.leadingAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.leadingAnchor
                       constant:kHorizontalInset],
    [mainStack.trailingAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.trailingAnchor
                       constant:-kHorizontalInset],
    [mainStack.bottomAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.bottomAnchor
                       constant:-kMainStackTopInset],
    [mainStack.widthAnchor
        constraintEqualToAnchor:scrollView.frameLayoutGuide.widthAnchor
                       constant:-2 * kHorizontalInset],

    [_jsonInputView.heightAnchor constraintEqualToConstant:150],
    [_responseContainer.heightAnchor constraintEqualToConstant:100],
    [_framesAndContentNodesContainer.heightAnchor
        constraintEqualToConstant:200],
  ]];
}

// Sets up the configuration for each tool. Note that templates default to a
// coordinate based approach.
- (void)setupToolConfigs {
  _toolConfigs = @{
    kToolMultiTool : @{
      @"ui" : @[ _tabIdContainer, _frameIdContainer, _jsonContainer ],
      @"template" : @[
        @{
          @"click" : @{
            @"tab_id" : @(0),
            @"target" : @{@"coordinate" : @{@"x" : @(100), @"y" : @(100)}},
            @"click_type" : @(1),
            @"click_count" : @(1)
          }
        },
        @{@"wait" : @{@"wait_time_ms" : @(3000), @"observe_tab_id" : @(0)}}, @{
          @"click" : @{
            @"tab_id" : @(0),
            @"target" : @{@"coordinate" : @{@"x" : @(200), @"y" : @(200)}},
            @"click_type" : @(1),
            @"click_count" : @(1)
          }
        }
      ]
    },
    kToolNavigate : @{
      @"ui" : @[ _tabIdContainer, _jsonContainer ],
      @"template" : @{
        @"navigate" :
            @{@"tab_id" : kTabIdMacro, @"url" : @"https://www.google.com"}
      }
    },
    kToolClick : @{
      @"ui" : @[ _tabIdContainer, _frameIdContainer, _jsonContainer ],
      @"template" : @{
        @"click" : @{
          @"tab_id" : kTabIdMacro,
          @"target" : @{@"coordinate" : @{@"x" : @(200), @"y" : @(200)}},
          @"click_type" : @(1),
          @"click_count" : @(1)
        }
      }
    },
    kToolType : @{
      @"ui" : @[ _tabIdContainer, _frameIdContainer, _jsonContainer ],
      @"template" : @{
        @"type" : @{
          @"tab_id" : kTabIdMacro,
          @"target" : @{@"coordinate" : @{@"x" : @(200), @"y" : @(200)}},
          @"text" : @"Foobarbaz",
          @"follow_by_enter" : @(NO),
          @"mode" : @(1),
        }
      }
    },
    kToolHistoryBack : @{
      @"ui" : @[ _tabIdContainer, _jsonContainer ],
      @"template" : @{@"back" : @{@"tab_id" : kTabIdMacro}}
    },
    kToolHistoryForward : @{
      @"ui" : @[ _tabIdContainer, _jsonContainer ],
      @"template" : @{@"forward" : @{@"tab_id" : kTabIdMacro}}
    },
    kToolWait : @{
      @"ui" : @[ _tabIdContainer, _jsonContainer ],
      @"template" : @{
        @"wait" : @{@"wait_time_ms" : @(3000), @"observe_tab_id" : kTabIdMacro}
      }
    },
    kToolScroll : @{
      @"ui" : @[ _tabIdContainer, _frameIdContainer, _jsonContainer ],
      @"template" : @{
        @"scroll" : @{
          @"tab_id" : kTabIdMacro,
          @"target" : @{@"coordinate" : @{@"x" : @(200), @"y" : @(200)}},
          @"direction" : @(4),
          @"distance" : @"123.45"
        }
      }
    },
    kToolScrollTo : @{
      @"ui" : @[ _tabIdContainer, _frameIdContainer, _jsonContainer ],
      @"template" : @{
        @"scroll_to" : @{
          @"tab_id" : kTabIdMacro,
          @"target" : @{@"coordinate" : @{@"x" : @(200), @"y" : @(200)}}
        }
      }
    },
    kToolSelect : @{
      @"ui" : @[ _tabIdContainer, _frameIdContainer, _jsonContainer ],
      @"template" : @{
        @"select" : @{
          @"tab_id" : kTabIdMacro,
          @"target" : @{@"coordinate" : @{@"x" : @(200), @"y" : @(200)}},
          @"value" : @"Option 1"
        }
      }
    },
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

// Helper to update a tool dictionary with tab ID and frame ID.
- (void)updateToolDict:(NSMutableDictionary*)toolDict
        configTemplate:(NSDictionary*)configTemplate
                 tabId:(NSString*)tabId
               frameId:(NSString*)frameId {
  if (![toolDict isKindOfClass:[NSMutableDictionary class]]) {
    return;
  }
  // 2. Update Tab ID.
  if (tabId) {
    toolDict[@"tab_id"] = @([tabId intValue]);
  }

  // 3. Update Target (Frame).
  if (toolDict[@"target"]) {
    if (frameId) {
      toolDict[@"target"] =
          [@{@"document_identifier" : frameId, @"content_node_id" : @(1)}
              mutableCopy];
    } else if (configTemplate && configTemplate[@"target"]) {
      // Revert to template default if frame deselected.
      toolDict[@"target"] = [configTemplate[@"target"] mutableCopy];
    }
  }
}

/**
 * Updates the JSON template for the specified tool with the given params.
 *
 * This parses the existing JSON (or loads the template if empty) and updates it
 * with the provided tab ID and frame ID. If frameId is unset, the `target` in
 * the default is used. If frameId is set, a document-identifier based target is
 * used.
 *
 * @param toolName The name of the tool (e.g. "Navigate", "Click").
 * @param tabId The tab ID to set in the JSON (can be nil).
 * @param frameId The frame ID to set in the JSON (can be nil).
 */
- (void)updateJsonInputForTool:(NSString*)toolName
                     withTabId:(NSString*)tabId
                       frameId:(NSString*)frameId {
  NSDictionary* config = _toolConfigs[toolName];
  if (!config) {
    return;
  }

  // 1. Loads input data from `_jsonInputView` or, if empty, the template.
  NSData* inputData;
  if (_jsonInputView.text.length == 0) {
    inputData = [NSJSONSerialization dataWithJSONObject:config[@"template"]
                                                options:0
                                                  error:nil];
  } else {
    inputData = [_jsonInputView.text dataUsingEncoding:NSUTF8StringEncoding];
  }

  // 2. Replace the tab ID placeholder.
  if (tabId) {
    NSMutableString* jsonString =
        [[NSMutableString alloc] initWithData:inputData
                                     encoding:NSUTF8StringEncoding];
    NSString* tabIdMacroAndQuotes =
        [NSString stringWithFormat:@"\"%@\"", kTabIdMacro];
    [jsonString replaceOccurrencesOfString:tabIdMacroAndQuotes
                                withString:tabId
                                   options:0
                                     range:NSMakeRange(0, [jsonString length])];
    inputData = [jsonString dataUsingEncoding:NSUTF8StringEncoding];
  }

  // 3. Parse the data into JSON format.
  NSError* error = nil;
  id jsonRoot =
      [NSJSONSerialization JSONObjectWithData:inputData
                                      options:NSJSONReadingMutableContainers
                                        error:&error];

  if (!jsonRoot) {
    _responseContainer.text =
        [NSString stringWithFormat:@"Failed to parse JSON: %@",
                                   error.localizedDescription];
    return;
  }

  NSString* toolKey = [toolName lowercaseString];

  if (![toolName isEqualToString:kToolMultiTool]) {
    if (![jsonRoot isKindOfClass:[NSMutableDictionary class]] ||
        ![jsonRoot count]) {
      _responseContainer.text = [NSString
          stringWithFormat:@"JSON mismatch: missing key '%@'", toolKey];
      return;
    }

    // Attempt to find the tool dictionary. Use the toolKey if present,
    // otherwise fallback to the first object.
    NSMutableDictionary* toolDict = jsonRoot[toolKey];
    if (!toolDict) {
      toolDict = [jsonRoot allValues].firstObject;
    }

    // Find the config template for target reversion.
    NSDictionary* configTemplate = config[@"template"][toolKey];
    if (!configTemplate &&
        [config[@"template"] isKindOfClass:[NSDictionary class]]) {
      configTemplate = [config[@"template"] allValues].firstObject;
    }

    [self updateToolDict:toolDict
          configTemplate:configTemplate
                   tabId:tabId
                 frameId:frameId];
  } else {
    if (![jsonRoot isKindOfClass:[NSMutableArray class]]) {
      _responseContainer.text =
          @"JSON mismatch: Multi-tool requires an array of actions.";
      return;
    }

    for (id action in (NSMutableArray*)jsonRoot) {
      if (![action isKindOfClass:[NSMutableDictionary class]]) {
        continue;
      }
      NSMutableDictionary* actionDict = (NSMutableDictionary*)action;
      NSString* actionKey = [[actionDict allKeys] firstObject];
      if (actionKey) {
        // Find default template for this action type.
        NSDictionary* defaultTemplate = nil;
        for (NSString* key in _toolConfigs) {
          NSDictionary* toolConfig = _toolConfigs[key];
          if ([toolConfig[@"template"] isKindOfClass:[NSDictionary class]] &&
              toolConfig[@"template"][actionKey]) {
            defaultTemplate = toolConfig[@"template"][actionKey];
            break;
          }
        }
        [self updateToolDict:actionDict[actionKey]
              configTemplate:defaultTemplate
                       tabId:tabId
                     frameId:frameId];
      }
    }
  }

  // 4. Serialize the JSON back to the input view.
  NSData* updatedData = [NSJSONSerialization
      dataWithJSONObject:jsonRoot
                 options:NSJSONWritingPrettyPrinted |
                         NSJSONWritingWithoutEscapingSlashes
                   error:nil];
  if (updatedData) {
    _jsonInputView.text = [[NSString alloc] initWithData:updatedData
                                                encoding:NSUTF8StringEncoding];
  }
}

- (void)selectTool:(NSString*)toolName {
  [_toolPickerButton setTitle:toolName forState:UIControlStateNormal];
  _jsonInputView.text = @"";
  NSString* effectiveTabId = _selectedTabId ?: _activeTabId;
  [self updateJsonInputForTool:toolName
                     withTabId:effectiveTabId
                       frameId:_selectedFrameId];

  _tabIdContainer.hidden = YES;
  _frameIdContainer.hidden = YES;
  _jsonContainer.hidden = YES;

  BOOL isWebActuationTool = IsWebActuationTool(toolName);
  _updateAPCDebugData.hidden = !isWebActuationTool;
  _framesAndContentNodesContainer.hidden = !isWebActuationTool;
  _framesAndContentNodesLabel.hidden = !isWebActuationTool;

  if (isWebActuationTool) {
    [self.mutator executeAPCExtractionWithRichExtraction:YES
                                          actionableMode:YES
                                        includeDebugData:YES];
  }

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

  // Define the explicit order for the dropdown menu.
  NSArray<NSString*>* orderedTools = @[
    kToolMultiTool, kToolNavigate, kToolClick, kToolType, kToolHistoryBack,
    kToolHistoryForward, kToolWait, kToolScroll, kToolScrollTo, kToolSelect
  ];

  for (NSString* toolName in orderedTools) {
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

- (void)onUpdateApcButtonPressed:(UIButton*)sender {
  _framesAndContentNodesContainer.text = @"Refreshing page context...";
  [self.mutator executeAPCExtractionWithRichExtraction:YES
                                        actionableMode:YES
                                      includeDebugData:YES];
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
                  NSString* effectiveTabId =
                      strongSelf->_selectedTabId ?: strongSelf->_activeTabId;
                  [strongSelf
                      updateJsonInputForTool:strongSelf->_toolPickerButton
                                                 .titleLabel.text
                                   withTabId:effectiveTabId
                                     frameId:strongSelf->_selectedFrameId];
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
  NSString* effectiveTabId = _selectedTabId ?: _activeTabId;
  [self updateJsonInputForTool:_toolPickerButton.titleLabel.text
                     withTabId:effectiveTabId
                       frameId:_selectedFrameId];
}

- (void)updateFrameList:(NSArray<NSDictionary*>*)frames {
  NSMutableArray<UIAction*>* actions = [NSMutableArray array];
  __weak __typeof(self) weakSelf = self;

  // Add an option to clear the frame selection
  UIAction* clearAction = [UIAction
      actionWithTitle:@"None (Use Coordinate)"
                image:nil
           identifier:nil
              handler:^(UIAction* menuAction) {
                __typeof(self) strongSelf = weakSelf;
                if (!strongSelf) {
                  return;
                }
                strongSelf->_selectedFrameId = nil;
                [strongSelf->_frameIdButton setTitle:@"Select Frame"
                                            forState:UIControlStateNormal];
                NSString* effectiveTabId =
                    strongSelf->_selectedTabId ?: strongSelf->_activeTabId;
                [strongSelf updateJsonInputForTool:strongSelf->_toolPickerButton
                                                       .titleLabel.text
                                         withTabId:effectiveTabId
                                           frameId:nil];
              }];
  [actions addObject:clearAction];

  for (NSDictionary* frame in frames) {
    NSString* docId = frame[@"document_identifier"];
    NSString* urlString = frame[@"url"];
    BOOL isMainFrame = [frame[@"is_main_frame"] boolValue];
    int depth = [frame[@"depth"] intValue];

    NSURL* url = [NSURL URLWithString:urlString];
    NSString* origin = url.host ? url.host : @"unknown origin";
    NSString* displayId = docId.length > 8 ? [docId substringToIndex:8] : docId;

    NSString* indent = [@"" stringByPaddingToLength:depth * 2
                                         withString:@" "
                                    startingAtIndex:0];

    NSString* displayTitle;
    if (isMainFrame) {
      displayTitle =
          [NSString stringWithFormat:@"Main: %@ (%@...)", origin, displayId];
    } else {
      displayTitle = [NSString
          stringWithFormat:@"%@Iframe: %@ (%@...)", indent, origin, displayId];
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
                  strongSelf->_selectedFrameId = docId;
                  [strongSelf->_frameIdButton setTitle:displayTitle
                                              forState:UIControlStateNormal];
                  NSString* effectiveTabId =
                      strongSelf->_selectedTabId ?: strongSelf->_activeTabId;
                  [strongSelf
                      updateJsonInputForTool:strongSelf->_toolPickerButton
                                                 .titleLabel.text
                                   withTabId:effectiveTabId
                                     frameId:docId];
                }];
    [actions addObject:action];
  }

  if (_frameMenuCompletion) {
    _frameMenuCompletion(actions);
    _frameMenuCompletion = nil;
  }
}

- (void)updateFramesAndContentNodesDebugString:
    (NSString*)framesAndContentNodes {
  _framesAndContentNodesContainer.text = framesAndContentNodes;
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

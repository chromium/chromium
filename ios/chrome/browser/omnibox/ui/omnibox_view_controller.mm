// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/omnibox_view_controller.h"

#import "base/containers/contains.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/omnibox/public/omnibox_constants.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_container_view.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_keyboard_delegate.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_mutator.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_input_delegate.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::UserMetricsAction;

@interface OmniboxViewController () <OmniboxTextInputDelegate,
                                     OmniboxKeyboardDelegate,
                                     UIScribbleInteractionDelegate>

// Override of UIViewController's view with a different type.
@property(nonatomic, strong) OmniboxContainerView* view;

// Whether the default search engine supports search-by-image. This controls the
// edit menu option to do an image search.
@property(nonatomic, assign) BOOL searchByImageEnabled;

// Whether the default search engine supports Lens. This controls the
// edit menu option to do a Lens search.
@property(nonatomic, assign) BOOL lensImageEnabled;

/// The placeholder text used in normal mode.
@property(nonatomic, copy) NSString* searchOrTypeURLPlaceholderText;
/// The placeholder text used in search-only mode.
@property(nonatomic, copy) NSString* searchOnlyPlaceholderText;

// YES if we are already forwarding an textDidChangeWithUserEvent message to the
// omnibox text controller. Needed to prevent infinite recursion.
// TODO(crbug.com/40103694): There must be a better way.
@property(nonatomic, assign) BOOL forwardingOnDidChange;

// YES if this text field is currently processing a user-initiated event,
// such as typing in the omnibox or pressing the clear button.  Used to
// distinguish between calls to textDidChange that are triggered by the user
// typing vs by calls to setText.
@property(nonatomic, assign) BOOL processingUserEvent;

// A flag that is set whenever any input or copy/paste event happened in the
// omnibox while it was focused. Used to count event "user focuses the omnibox
// to view the complete URL and immediately defocuses it".
@property(nonatomic, assign) BOOL omniboxInteractedWhileFocused;

// Stores whether the clipboard currently stores copied content.
@property(nonatomic, assign) BOOL hasCopiedContent;
// Stores the current content type in the clipboard. This is only valid if
// `hasCopiedContent` is YES.
@property(nonatomic, assign) ClipboardContentType copiedContentType;
// Stores whether the cached clipboard state is currently being updated. See
// `-updateCachedClipboardState` for more information.
@property(nonatomic, assign) BOOL isUpdatingCachedClipboardState;

@end

@implementation OmniboxViewController {
  // Omnibox uses a custom clear button. It has a custom tint and image, but
  // otherwise it should act exactly like a system button.
  /// Clear button owned by `view` (OmniboxContainerView).
  __weak UIButton* _clearButton;

  /// The context in which the omnibox is presented.
  OmniboxPresentationContext _presentationContext;
}

@dynamic view;

- (instancetype)initWithPresentationContext:
    (OmniboxPresentationContext)presentationContext {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _presentationContext = presentationContext;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  UIColor* textColor = [UIColor colorNamed:kTextPrimaryColor];
  UIColor* textInputTintColor = [UIColor colorNamed:kBlueColor];
  UIColor* iconTintColor;
  iconTintColor = [UIColor colorNamed:kToolbarButtonColor];

  self.view = [[OmniboxContainerView alloc] initWithFrame:CGRectZero
                                                textColor:textColor
                                            textInputTint:textInputTintColor
                                                 iconTint:iconTintColor
                                      presentationContext:_presentationContext];
  self.view.layoutGuideCenter = self.layoutGuideCenter;
  self.view.metricsRecorder = self.metricsRecorder;
  _clearButton = self.view.clearButton;

  self.view.shouldGroupAccessibilityChildren = YES;

  self.textInput.omniboxTextInputDelegate = self;
  self.textInput.omniboxKeyboardDelegate = self;

  SetA11yLabelAndUiAutomationName(self.textInput.view, IDS_ACCNAME_LOCATION,
                                  @"Address");

  [self.textInput.view
      addInteraction:[[UIScribbleInteraction alloc] initWithDelegate:self]];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self.textInput setDefaultPlaceholderText:[self currentPlaceholderText]];

  [_clearButton addTarget:self
                   action:@selector(clearButtonPressed)
         forControlEvents:UIControlEventTouchUpInside];

  if (base::FeatureList::IsEnabled(kEnableLensOverlay)) {
    [self.view.thumbnailButton addTarget:self
                                  action:@selector(didTapThumbnailButton)
                        forControlEvents:UIControlEventTouchUpInside];
  }

  [NSNotificationCenter.defaultCenter
      addObserver:self
         selector:@selector(textInputModeDidChange)
             name:UITextInputCurrentInputModeDidChangeNotification
           object:nil];

  // Reset the text after initial layout has been forced, see comment in
  // `OmniboxTextFieldIOS`.
  if ([self.textInput.text isEqualToString:@" "]) {
    self.textInput.text = @"";
  }
  [self updateClearButtonVisibility];
  [self updateLeadingImage];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  [NSNotificationCenter.defaultCenter
      addObserver:self
         selector:@selector(pasteboardDidChange:)
             name:UIPasteboardChangedNotification
           object:nil];

  // The pasteboard changed notification doesn't fire if the clipboard changes
  // while the app is in the background, so update the state whenever the app
  // becomes active.
  [NSNotificationCenter.defaultCenter
      addObserver:self
         selector:@selector(applicationDidBecomeActive:)
             name:UIApplicationDidBecomeActiveNotification
           object:nil];
}

- (void)viewIsAppearing:(BOOL)animated {
  [super viewIsAppearing:animated];
  if (_presentationContext == OmniboxPresentationContext::kLensOverlay) {
    self.semanticContentAttribute =
        [self.textInput bestSemanticContentAttribute];
    [self.textInput updateTextDirection];
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  if (_presentationContext == OmniboxPresentationContext::kComposebox) {
    [self.view updateTextViewHeight];
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  self.textInput.selectedTextRange =
      [self.textInput textRangeFromPosition:self.textInput.beginningOfDocument
                                 toPosition:self.textInput.beginningOfDocument];

  [NSNotificationCenter.defaultCenter
      removeObserver:self
                name:UIPasteboardChangedNotification
              object:nil];

  // The pasteboard changed notification doesn't fire if the clipboard changes
  // while the app is in the background, so update the state whenever the app
  // becomes active.
  [NSNotificationCenter.defaultCenter
      removeObserver:self
                name:UIApplicationDidBecomeActiveNotification
              object:nil];
}

#pragma mark - properties

- (UIView<TextFieldViewContaining>*)viewContainingTextField {
  return self.view;
}

- (void)setMetricsRecorder:(OmniboxMetricsRecorder*)metricsRecorder {
  _metricsRecorder = metricsRecorder;
  self.view.metricsRecorder = metricsRecorder;
}

#pragma mark - public methods

- (id<OmniboxTextInput>)textInput {
  return self.view.textInput;
}

- (void)prepareOmniboxForScribble {
  [self.mutator prepareForScribble];
  [self.textInput setDefaultPlaceholderText:nil];
}

- (void)cleanupOmniboxAfterScribble {
  [self.mutator cleanupAfterScribble];
  [self.textInput setDefaultPlaceholderText:[self currentPlaceholderText]];
}

#pragma mark - OmniboxTextInputDelegate

- (BOOL)textInput:(id<OmniboxTextInput>)textInput
    shouldChangeTextInRange:(NSRange)range
          replacementString:(NSString*)newText {
  // Any change in the content of the omnibox should deselect thumbnail button.
  self.view.thumbnailButton.selected = NO;
  self.processingUserEvent =
      [self.mutator shouldChangeCharactersInRange:range
                                replacementString:newText];
  return self.processingUserEvent;
}

- (void)textInputDidChange:(id<OmniboxTextInput>)textInput {
  [self updateLeadingImage];
  [self updateClearButtonVisibility];
  self.semanticContentAttribute = [self.textInput bestSemanticContentAttribute];

  if (self.forwardingOnDidChange) {
    return;
  }

  // Reset the changed flag.
  self.omniboxInteractedWhileFocused = YES;

  BOOL savedProcessingUserEvent = self.processingUserEvent;
  self.processingUserEvent = NO;
  self.forwardingOnDidChange = YES;
  [self.mutator textDidChangeWithUserEvent:savedProcessingUserEvent];
  self.forwardingOnDidChange = NO;
}

- (BOOL)textInputShouldReturn:(id<OmniboxTextInput>)textInput {
  // Forward kReturnKey action to the keyboard handler.
  if ([self canPerformKeyboardAction:OmniboxKeyboardAction::kReturnKey]) {
    [self performKeyboardAction:OmniboxKeyboardAction::kReturnKey];
    return NO;
  }
  return YES;
}

// Always update the text field colors when we start editing.  It's possible
// for this method to be called when we are already editing (popup focus
// change).  In this case, OnDidBeginEditing will be called multiple times.
// If that becomes an issue a boolean should be added to track editing state.
- (void)textInputDidBeginEditing:(id<OmniboxTextInput>)textInput {
  [self updateCachedClipboardState];

  // Update the clear button state.
  [self updateClearButtonVisibility];
  [self updateLeadingImage];

  if (base::FeatureList::IsEnabled(kEnableLensOverlay)) {
    self.view.thumbnailButton.selected = NO;
  }

  self.semanticContentAttribute = [self.textInput bestSemanticContentAttribute];

  self.omniboxInteractedWhileFocused = NO;
  [self.mutator onDidBeginEditing];
}

// Records the metrics as needed.
- (void)textInputDidEndEditing:(id<OmniboxTextInput>)textInput {
  if (base::FeatureList::IsEnabled(kEnableLensOverlay)) {
    self.view.thumbnailButton.selected = NO;
  }

  if (!self.omniboxInteractedWhileFocused) {
    RecordAction(
        UserMetricsAction("Mobile_FocusedDefocusedOmnibox_WithNoAction"));
  }
}

- (UIMenu*)textInput:(id<OmniboxTextInput>)textInput
    editMenuForCharactersInRange:(NSRange)range
                suggestedActions:(NSArray<UIMenuElement*>*)suggestedActions {
  NSMutableArray* actions = [suggestedActions mutableCopy];
  if ([self canPerformAction:@selector(searchCopiedImage:) withSender:nil]) {
    UIAction* searchCopiedImage = [UIAction
        actionWithTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_COPIED_IMAGE)
                  image:nil
             identifier:nil
                handler:^(__kindof UIAction* action) {
                  [self searchCopiedImage:nil];
                }];
    [actions addObject:searchCopiedImage];
  }

  if ([self canPerformAction:@selector(lensCopiedImage:) withSender:nil]) {
    UIAction* searchCopiedImageWithLens =
        [UIAction actionWithTitle:l10n_util::GetNSString(
                                      IDS_IOS_SEARCH_COPIED_IMAGE_WITH_LENS)
                            image:nil
                       identifier:nil
                          handler:^(__kindof UIAction* action) {
                            [self lensCopiedImage:nil];
                          }];
    [actions addObject:searchCopiedImageWithLens];
  }

  if ([self canPerformAction:@selector(visitCopiedLink:) withSender:nil]) {
    UIAction* visitCopiedLink = [UIAction
        actionWithTitle:l10n_util::GetNSString(IDS_IOS_VISIT_COPIED_LINK)
                  image:nil
             identifier:nil
                handler:^(__kindof UIAction* action) {
                  [self visitCopiedLink:nil];
                }];
    [actions addObject:visitCopiedLink];
  }

  if ([self canPerformAction:@selector(searchCopiedText:) withSender:nil]) {
    UIAction* searchCopiedText = [UIAction
        actionWithTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_COPIED_TEXT)
                  image:nil
             identifier:nil
                handler:^(__kindof UIAction* action) {
                  [self searchCopiedText:nil];
                }];
    [actions addObject:searchCopiedText];
  }

  return [UIMenu menuWithChildren:actions];
}

- (void)textInputDidCopy:(id<OmniboxTextInput>)textInput {
  self.omniboxInteractedWhileFocused = YES;
  [self.mutator onCopy];
}

- (void)textInputWillPaste:(id<OmniboxTextInput>)textInput {
  [self.mutator willPaste];
}

- (void)textInputDidDeleteBackward:(id<OmniboxTextInput>)textInput {
  // If not in pre-edit, deleting when cursor is at the beginning interacts with
  // the thumbnail.
  if (!textInput.isPreEditing && textInput.selectedTextRange.empty &&
      [textInput offsetFromPosition:textInput.beginningOfDocument
                         toPosition:textInput.selectedTextRange.start] == 0) {
    [self didTapThumbnailButton];
  }
  [self.mutator onDeleteBackward];
}

- (void)textInputDidAcceptAutocomplete:(id<OmniboxTextInput>)textInput {
  [self.mutator onAcceptAutocomplete];
}

- (void)textInputDidRemoveAdditionalText:(id<OmniboxTextInput>)textInput {
  base::RecordAction(UserMetricsAction("MobileOmniboxRichInlineRemoved"));
  [self.mutator removeAdditionalText];
}

- (BOOL)textInput:(id<OmniboxTextInput>)textInput
    canPasteItemProviders:(NSArray<NSItemProvider*>*)itemProviders {
  for (NSItemProvider* itemProvider in itemProviders) {
    if (((self.searchByImageEnabled || self.shouldUseLensInMenu) &&
         [itemProvider canLoadObjectOfClass:[UIImage class]]) ||
        [itemProvider canLoadObjectOfClass:[NSURL class]] ||
        [itemProvider canLoadObjectOfClass:[NSString class]]) {
      return YES;
    }
  }
  return NO;
}

- (void)textInput:(id<OmniboxTextInput>)textInput
    pasteItemProviders:(NSArray<NSItemProvider*>*)itemProviders {
  // Interacted while focused.
  self.omniboxInteractedWhileFocused = YES;

  [self.mutator pasteToSearch:itemProviders];
}

- (void)textInputDidAcceptInput:(id<OmniboxTextInput>)textInput {
  [self.mutator acceptInput];
}

#pragma mark - OmniboxKeyboardDelegate

- (BOOL)canPerformKeyboardAction:(OmniboxKeyboardAction)keyboardAction {
  return [self.popupKeyboardDelegate canPerformKeyboardAction:keyboardAction] ||
         [self.textInput canPerformKeyboardAction:keyboardAction];
}

- (void)performKeyboardAction:(OmniboxKeyboardAction)keyboardAction {
  if ([self.popupKeyboardDelegate canPerformKeyboardAction:keyboardAction]) {
    [self.popupKeyboardDelegate performKeyboardAction:keyboardAction];
  } else if ([self.textInput canPerformKeyboardAction:keyboardAction]) {
    [self.textInput performKeyboardAction:keyboardAction];
  } else {
    NOTREACHED() << "Check canPerformKeyboardAction before!";
  }
}

#pragma mark - OmniboxConsumer

- (void)updateAutocompleteIcon:(UIImage*)icon
    withAccessibilityIdentifier:(NSString*)accessibilityIdentifier {
  [self.view setLeadingImage:icon
      withAccessibilityIdentifier:accessibilityIdentifier];
}
- (void)updateSearchByImageSupported:(BOOL)searchByImageSupported {
  self.searchByImageEnabled = searchByImageSupported;
}

- (void)updateLensImageSupported:(BOOL)lensImageSupported {
  self.lensImageEnabled = lensImageSupported;
}

- (void)setThumbnailImage:(UIImage*)image {
  [self.view setThumbnailImage:image];
  // Cancel any pending image removal if a new selection is made.
  self.view.thumbnailButton.selected = NO;
  [self.textInput setDefaultPlaceholderText:[self currentPlaceholderText]];
  [self updateReturnKeyAvailability];
}

- (void)updateReturnKeyAvailability {
  self.textInput.allowsReturnKeyWithEmptyText =
      !!self.view.thumbnailImage ||
      [self.popupKeyboardDelegate
          canPerformKeyboardAction:OmniboxKeyboardAction::kReturnKey];
}

- (void)setPlaceholderText:(NSString*)placeholderText {
  if (_searchOrTypeURLPlaceholderText == placeholderText) {
    return;
  }
  _searchOrTypeURLPlaceholderText = [placeholderText copy];

  [self.textInput setDefaultPlaceholderText:[self currentPlaceholderText]];
}

- (void)setSearchOnlyPlaceholderText:(NSString*)placeholderText {
  if (_searchOnlyPlaceholderText == placeholderText) {
    return;
  }
  _searchOnlyPlaceholderText = [placeholderText copy];
  [self.textInput setDefaultPlaceholderText:[self currentPlaceholderText]];
}

#pragma mark - EditViewAnimatee

- (void)setLeadingIconScale:(CGFloat)scale {
  [self.view setLeadingImageScale:scale];
}

- (void)setClearButtonFaded:(BOOL)faded {
  _clearButton.alpha = faded ? 0 : 1;
}

#pragma mark - LocationBarOffsetProvider

- (CGFloat)xOffsetForString:(NSString*)string {
  return [self.textInput offsetForString:string];
}

#pragma mark - private

- (void)updateLeadingImage {
  UIImage* image = self.textInput.text.length ? self.defaultLeadingImage
                                              : self.emptyTextLeadingImage;
  NSString* accessibilityID =
      self.textInput.text.length
          ? kOmniboxLeadingImageDefaultAccessibilityIdentifier
          : kOmniboxLeadingImageEmptyTextAccessibilityIdentifier;

  [self.view setLeadingImage:image withAccessibilityIdentifier:accessibilityID];
}

- (BOOL)shouldUseLensInMenu {
  return ios::provider::IsLensSupported() &&
         base::FeatureList::IsEnabled(kEnableLensInOmniboxCopiedImage) &&
         self.lensImageEnabled;
}

- (void)onClipboardContentTypesReceived:
    (const std::set<ClipboardContentType>&)types {
  self.hasCopiedContent = !types.empty();
  if ((self.searchByImageEnabled || self.shouldUseLensInMenu) &&
      base::Contains(types, ClipboardContentType::Image)) {
    self.copiedContentType = ClipboardContentType::Image;
  } else if (base::Contains(types, ClipboardContentType::URL)) {
    self.copiedContentType = ClipboardContentType::URL;
  } else if (base::Contains(types, ClipboardContentType::Text)) {
    self.copiedContentType = ClipboardContentType::Text;
  }
  self.isUpdatingCachedClipboardState = NO;
}

#pragma mark notification callbacks

// Called on UITextInputCurrentInputModeDidChangeNotification for self.textInput
- (void)textInputModeDidChange {
  // Only respond to language changes when the omnibox is first responder.
  if (![self.textInput.view isFirstResponder]) {
    return;
  }

  [self.textInput updateTextDirection];
  self.semanticContentAttribute = [self.textInput bestSemanticContentAttribute];

  [self.mutator onTextInputModeChange];
}

- (void)updateCachedClipboardState {
  // Sometimes, checking the clipboard state itself causes the clipboard to
  // emit a UIPasteboardChangedNotification, leading to an infinite loop. For
  // now, just prevent re-checking the clipboard state, but hopefully this will
  // be fixed in a future iOS version (see crbug.com/1049053 for crash details).
  if (self.isUpdatingCachedClipboardState) {
    return;
  }
  self.isUpdatingCachedClipboardState = YES;
  self.hasCopiedContent = NO;
  ClipboardRecentContent* clipboardRecentContent =
      ClipboardRecentContent::GetInstance();
  std::set<ClipboardContentType> desired_types;
  desired_types.insert(ClipboardContentType::URL);
  desired_types.insert(ClipboardContentType::Text);
  desired_types.insert(ClipboardContentType::Image);
  __weak __typeof(self) weakSelf = self;
  clipboardRecentContent->HasRecentContentFromClipboard(
      desired_types,
      base::BindOnce(^(std::set<ClipboardContentType> matched_types) {
        [weakSelf onClipboardContentTypesReceived:matched_types];
      }));
}

- (void)pasteboardDidChange:(NSNotification*)notification {
  [self updateCachedClipboardState];
}

- (void)applicationDidBecomeActive:(NSNotification*)notification {
  [self updateCachedClipboardState];
}

#pragma mark clear button

- (void)clearButtonPressed {
  [self.mutator clearText];
  [self updateClearButtonVisibility];
  [self updateLeadingImage];
}

// Hides the clear button if the textfield is empty; shows it otherwise.
- (void)updateClearButtonVisibility {
  BOOL hasText = self.textInput.text.length > 0;
  [self.view setClearButtonHidden:!hasText];
}

// Handle the updates to semanticContentAttribute by passing the changes along
// to the necessary views.
- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  _semanticContentAttribute = semanticContentAttribute;

  self.view.semanticContentAttribute = self.semanticContentAttribute;
  self.textInput.view.semanticContentAttribute = self.semanticContentAttribute;
}

#pragma mark - UIMenuItem

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (action == @selector(searchCopiedImage:) ||
      action == @selector(lensCopiedImage:) ||
      action == @selector(visitCopiedLink:) ||
      action == @selector(searchCopiedText:)) {
    if (!self.hasCopiedContent) {
      return NO;
    }
    if (self.copiedContentType == ClipboardContentType::Image) {
      if (self.shouldUseLensInMenu) {
        return action == @selector(lensCopiedImage:);
      }
      return action == @selector(searchCopiedImage:);
    }
    if (self.copiedContentType == ClipboardContentType::URL) {
      return action == @selector(visitCopiedLink:);
    }
    if (self.copiedContentType == ClipboardContentType::Text) {
      return action == @selector(searchCopiedText:);
    }
    return NO;
  }
  return NO;
}

- (void)searchCopiedImage:(id)sender {
  RecordAction(
      UserMetricsAction("Mobile.OmniboxContextMenu.SearchCopiedImage"));
  self.omniboxInteractedWhileFocused = YES;
  [self.mutator searchCopiedImage];
}

- (void)lensCopiedImage:(id)sender {
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.LensCopiedImage"));
  self.omniboxInteractedWhileFocused = YES;
  [self.mutator lensCopiedImage];
}

- (void)visitCopiedLink:(id)sender {
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.VisitCopiedLink"));
  self.omniboxInteractedWhileFocused = YES;
  [self.mutator visitCopiedLink];
}

- (void)searchCopiedText:(id)sender {
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.SearchCopiedText"));
  self.omniboxInteractedWhileFocused = YES;
  [self.mutator searchCopiedText];
}

#pragma mark - UIScribbleInteractionDelegate

- (void)scribbleInteractionWillBeginWriting:
    (UIScribbleInteraction*)interaction {
  [self.mutator prepareForScribble];
}

- (void)scribbleInteractionDidFinishWriting:
    (UIScribbleInteraction*)interaction {
  [self cleanupOmniboxAfterScribble];
}

/// Handles interaction with the thumbnail button. (tap or keyboard delete)
- (void)didTapThumbnailButton {
  if (!self.view.thumbnailButton.selected &&
      !self.view.thumbnailButton.accessibilityElementIsFocused) {
    self.view.thumbnailButton.selected = YES;
  } else {
    [self.mutator removeThumbnail];
    // Clear the selection once it's no longer needed. This prevents it from
    // reappearing unexpectedly as the user navigates back through previous
    // results.
    self.view.thumbnailButton.selected = NO;
  }
}

/// Returns the placeholder text for the current state.
- (NSString*)currentPlaceholderText {
  if (!base::FeatureList::IsEnabled(kEnableLensOverlay)) {
    return self.searchOrTypeURLPlaceholderText;
  }

  if (self.view.thumbnailImage) {
    return l10n_util::GetNSString(IDS_IOS_OMNIBOX_PLACEHOLDER_IMAGE_SEARCH);
  } else if (self.searchOnlyUI) {
    return self.searchOnlyPlaceholderText;
  } else {
    return self.searchOrTypeURLPlaceholderText;
  }
}

@end

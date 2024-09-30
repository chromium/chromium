// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_view_controller.h"

#import "base/containers/contains.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_container_view.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_keyboard_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_change_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::UserMetricsAction;

@interface OmniboxViewController () <OmniboxTextFieldDelegate,
                                     OmniboxKeyboardDelegate,
                                     UIScribbleInteractionDelegate> {
  // Weak, acts as a delegate
  raw_ptr<OmniboxTextChangeDelegate> _textChangeDelegate;
}

// Override of UIViewController's view with a different type.
@property(nonatomic, strong) OmniboxContainerView* view;

// Whether the default search engine supports search-by-image. This controls the
// edit menu option to do an image search.
@property(nonatomic, assign) BOOL searchByImageEnabled;

// Whether the default search engine supports Lens. This controls the
// edit menu option to do a Lens search.
@property(nonatomic, assign) BOOL lensImageEnabled;

/// The short name of the search provider.
@property(nonatomic, assign) std::u16string searchProviderName;

// YES if we are already forwarding an OnDidChange() message to the edit view.
// Needed to prevent infinite recursion.
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

// Tracks editing status, because only the omnibox that is in edit mode can
// get an edit menu.
@property(nonatomic, assign) BOOL isTextfieldEditing;

// Is YES while fixing display of edit menu (below omnibox).
@property(nonatomic, assign) BOOL showingEditMenu;

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

  /// Whether the view is presented in the lens overlay.
  BOOL _isLensOverlay;
}

@dynamic view;

- (instancetype)initWithIsLensOverlay:(BOOL)isLensOverlay {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _isLensOverlay = isLensOverlay;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  UIColor* textColor = [UIColor colorNamed:kTextPrimaryColor];
  UIColor* textFieldTintColor = [UIColor colorNamed:kBlueColor];
  UIColor* iconTintColor;
  iconTintColor = [UIColor colorNamed:kToolbarButtonColor];

  self.view = [[OmniboxContainerView alloc] initWithFrame:CGRectZero
                                                textColor:textColor
                                            textFieldTint:textFieldTintColor
                                                 iconTint:iconTintColor
                                            isLensOverlay:_isLensOverlay];
  self.view.layoutGuideCenter = self.layoutGuideCenter;
  _clearButton = self.view.clearButton;

  self.view.shouldGroupAccessibilityChildren = YES;

  self.textField.delegate = self;
  self.textField.omniboxKeyboardDelegate = self;

  SetA11yLabelAndUiAutomationName(self.textField, IDS_ACCNAME_LOCATION,
                                  @"Address");

  [self.textField
      addInteraction:[[UIScribbleInteraction alloc] initWithDelegate:self]];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.textField.placeholder = [self placeholderText];

  [_clearButton addTarget:self
                   action:@selector(clearButtonPressed)
         forControlEvents:UIControlEventTouchUpInside];

  // Observe text changes to show the clear button when there is text and hide
  // it when the textfield is empty.
  [self.textField addTarget:self
                     action:@selector(textFieldDidChange:)
           forControlEvents:UIControlEventEditingChanged];

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
  if ([self.textField.text isEqualToString:@" "]) {
    self.textField.text = @"";
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

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  self.textField.selectedTextRange =
      [self.textField textRangeFromPosition:self.textField.beginningOfDocument
                                 toPosition:self.textField.beginningOfDocument];

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

- (void)setTextChangeDelegate:(OmniboxTextChangeDelegate*)textChangeDelegate {
  _textChangeDelegate = textChangeDelegate;
}

- (void)setIsTextfieldEditing:(BOOL)owns {
  if (_isTextfieldEditing == owns) {
    return;
  }
  _isTextfieldEditing = owns;
}

- (UIView<TextFieldViewContaining>*)viewContainingTextField {
  return self.view;
}

#pragma mark - public methods

- (OmniboxTextFieldIOS*)textField {
  return self.view.textField;
}

- (void)prepareOmniboxForScribble {
  [self.textField exitPreEditState];
  [self.textField setText:[[NSAttributedString alloc] initWithString:@""]
           userTextLength:0];
  self.textField.placeholder = nil;
}

- (void)cleanupOmniboxAfterScribble {
  self.textField.placeholder = [self placeholderText];
}

#pragma mark - OmniboxTextFieldDelegate

- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)newText {
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return YES;
  }

  // Any change in the content of the omnibox should deselect thumbnail button.
  self.view.thumbnailButton.selected = NO;
  self.processingUserEvent = _textChangeDelegate->OnWillChange(range, newText);
  return self.processingUserEvent;
}

- (void)textFieldDidChange:(id)sender {
  [self updateLeadingImage];
  [self updateClearButtonVisibility];
  self.semanticContentAttribute = [self.textField bestSemanticContentAttribute];

  if (self.forwardingOnDidChange) {
    return;
  }

  // Reset the changed flag.
  self.omniboxInteractedWhileFocused = YES;

  BOOL savedProcessingUserEvent = self.processingUserEvent;
  self.processingUserEvent = NO;
  self.forwardingOnDidChange = YES;
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return;
  }
  _textChangeDelegate->OnDidChange(savedProcessingUserEvent);
  self.forwardingOnDidChange = NO;
}

// Delegate method for UITextField, called when user presses the "go" button.
- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  if (!self.returnKeyDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return YES;
  }
  [self.returnKeyDelegate omniboxReturnPressed:self];
  return NO;
}

// Always update the text field colors when we start editing.  It's possible
// for this method to be called when we are already editing (popup focus
// change).  In this case, OnDidBeginEditing will be called multiple times.
// If that becomes an issue a boolean should be added to track editing state.
- (void)textFieldDidBeginEditing:(UITextField*)textField {
  [self updateCachedClipboardState];

  // Update the clear button state.
  [self updateClearButtonVisibility];
  [self updateLeadingImage];

  if (base::FeatureList::IsEnabled(kEnableLensOverlay)) {
    self.view.thumbnailButton.selected = NO;
  }

  self.semanticContentAttribute = [self.textField bestSemanticContentAttribute];
  self.isTextfieldEditing = YES;

  self.omniboxInteractedWhileFocused = NO;
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return;
  }
  _textChangeDelegate->OnDidBeginEditing();
}

// Record the metrics as needed.
- (void)textFieldDidEndEditing:(UITextField*)textField
                        reason:(UITextFieldDidEndEditingReason)reason {
  self.isTextfieldEditing = NO;

  if (base::FeatureList::IsEnabled(kEnableLensOverlay)) {
    self.view.thumbnailButton.selected = NO;
  }

  if (!self.omniboxInteractedWhileFocused) {
    RecordAction(
        UserMetricsAction("Mobile_FocusedDefocusedOmnibox_WithNoAction"));
  }
}

- (BOOL)textFieldShouldClear:(UITextField*)textField {
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return YES;
  }
  _textChangeDelegate->ClearText();
  self.processingUserEvent = YES;
  return YES;
}

- (void)onCopy {
  self.omniboxInteractedWhileFocused = YES;
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return;
  }
  _textChangeDelegate->OnCopy();
}

- (void)willPaste {
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return;
  }
  _textChangeDelegate->WillPaste();
}

- (void)onDeleteBackward {
  // If not in pre-edit, deleting when cursor is at the beginning interacts with
  // the thumbnail.
  if (OmniboxTextFieldIOS* textField = self.textField;
      !textField.isPreEditing && textField.selectedTextRange.empty &&
      [textField offsetFromPosition:textField.beginningOfDocument
                         toPosition:textField.selectedTextRange.start] == 0) {
    [self didTapThumbnailButton];
  }
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return;
  }
  _textChangeDelegate->OnDeleteBackward();
}

- (void)textFieldDidAcceptAutocomplete:(OmniboxTextFieldIOS*)textField {
  if (_textChangeDelegate) {
    _textChangeDelegate->OnAcceptAutocomplete();
  }
}

- (void)textFieldDidRemoveAdditionalText:(OmniboxTextFieldIOS*)textField {
  base::RecordAction(UserMetricsAction("MobileOmniboxRichInlineRemoved"));
  if (_textChangeDelegate) {
    _textChangeDelegate->OnRemoveAdditionalText();
  }
}

- (BOOL)canPasteItemProviders:(NSArray<NSItemProvider*>*)itemProviders {
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

- (void)pasteItemProviders:(NSArray<NSItemProvider*>*)itemProviders {
  // Interacted while focused.
  self.omniboxInteractedWhileFocused = YES;

  [self.pasteDelegate didTapPasteToSearchButton:itemProviders];
}

- (BOOL)canPerformKeyboardAction:(OmniboxKeyboardAction)keyboardAction {
  return [self.popupKeyboardDelegate canPerformKeyboardAction:keyboardAction] ||
         [self.textField canPerformKeyboardAction:keyboardAction];
}

- (void)performKeyboardAction:(OmniboxKeyboardAction)keyboardAction {
  if ([self.popupKeyboardDelegate canPerformKeyboardAction:keyboardAction]) {
    if (keyboardAction == OmniboxKeyboardActionUpArrow ||
        keyboardAction == OmniboxKeyboardActionDownArrow) {
      [self.textField exitPreEditState];
    }
    [self.popupKeyboardDelegate performKeyboardAction:keyboardAction];
  } else if ([self.textField canPerformKeyboardAction:keyboardAction]) {
    [self.textField performKeyboardAction:keyboardAction];
  } else {
    NOTREACHED_IN_MIGRATION() << "Check canPerformKeyboardAction before!";
  }
}

- (UIMenu*)textField:(UITextField*)textField
    editMenuForCharactersInRange:(NSRange)range
                suggestedActions:(NSArray<UIMenuElement*>*)suggestedActions {
  NSMutableArray* actions = [suggestedActions mutableCopy];
  if ([self canPerformAction:@selector(searchCopiedImage:) withSender:nil]) {
    UIAction* searchCopiedImage = [UIAction
        actionWithTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_COPIED_IMAGE)
                  image:nil
             identifier:nil
                handler:^(__kindof UIAction* _Nonnull action) {
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
                          handler:^(__kindof UIAction* _Nonnull action) {
                            [self lensCopiedImage:nil];
                          }];
    [actions addObject:searchCopiedImageWithLens];
  }

  if ([self canPerformAction:@selector(visitCopiedLink:) withSender:nil]) {
    UIAction* visitCopiedLink = [UIAction
        actionWithTitle:l10n_util::GetNSString(IDS_IOS_VISIT_COPIED_LINK)
                  image:nil
             identifier:nil
                handler:^(__kindof UIAction* _Nonnull action) {
                  [self visitCopiedLink:nil];
                }];
    [actions addObject:visitCopiedLink];
  }

  if ([self canPerformAction:@selector(searchCopiedText:) withSender:nil]) {
    UIAction* searchCopiedText = [UIAction
        actionWithTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_COPIED_TEXT)
                  image:nil
             identifier:nil
                handler:^(__kindof UIAction* _Nonnull action) {
                  [self searchCopiedText:nil];
                }];
    [actions addObject:searchCopiedText];
  }

  return [UIMenu menuWithChildren:actions];
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

- (void)updateText:(NSAttributedString*)text {
  [self.textField setText:text userTextLength:text.length];
}

#pragma mark - OmniboxViewConsumer

- (void)updateAdditionalText:(NSString*)additionalText {
  [self.view updateAdditionalText:additionalText];
}

- (void)setOmniboxHasRichInline:(BOOL)omniboxHasRichInline {
  [self.view setOmniboxHasRichInline:omniboxHasRichInline];
}

- (void)setThumbnailImage:(UIImage*)image {
  [self.view setThumbnailImage:image];
  // Cancel any pending image removal if a new selection is made.
  self.view.thumbnailButton.selected = NO;
  self.textField.allowsReturnKeyWithEmptyText = !!image;
  self.textField.placeholder = [self placeholderText];
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
  return [self.textField offsetForString:string];
}

#pragma mark - private

- (void)updateLeadingImage {
  UIImage* image = self.textField.text.length ? self.defaultLeadingImage
                                              : self.emptyTextLeadingImage;
  NSString* accessibilityID =
      self.textField.text.length
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

// Called on UITextInputCurrentInputModeDidChangeNotification for self.textField
- (void)textInputModeDidChange {
  // Only respond to language changes when the omnibox is first responder.
  if (![self.textField isFirstResponder]) {
    return;
  }

  [self.textField updateTextDirection];
  self.semanticContentAttribute = [self.textField bestSemanticContentAttribute];

  [self.textInputDelegate omniboxViewControllerTextInputModeDidChange:self];
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
  // Emulate a system button clear callback.
  BOOL shouldClear =
      [self.textField.delegate textFieldShouldClear:self.textField];
  if (shouldClear) {
    [self.textField setText:@""];
    // Calling setText: does not trigger UIControlEventEditingChanged, so update
    // the clear button visibility manually.
    [self.textField sendActionsForControlEvents:UIControlEventEditingChanged];
  }
}

// Hides the clear button if the textfield is empty; shows it otherwise.
- (void)updateClearButtonVisibility {
  BOOL hasText = self.textField.text.length > 0;
  [self.view setClearButtonHidden:!hasText];
}

// Handle the updates to semanticContentAttribute by passing the changes along
// to the necessary views.
- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  _semanticContentAttribute = semanticContentAttribute;

  self.view.semanticContentAttribute = self.semanticContentAttribute;
  self.textField.semanticContentAttribute = self.semanticContentAttribute;
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
  [self.pasteDelegate didTapSearchCopiedImage];
}

- (void)lensCopiedImage:(id)sender {
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.LensCopiedImage"));
  self.omniboxInteractedWhileFocused = YES;
  [self.pasteDelegate didTapLensCopiedImage];
}

- (void)visitCopiedLink:(id)sender {
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.VisitCopiedLink"));
  self.omniboxInteractedWhileFocused = YES;
  [self.pasteDelegate didTapVisitCopiedLink];
}

- (void)searchCopiedText:(id)sender {
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.SearchCopiedText"));
  self.omniboxInteractedWhileFocused = YES;
  [self.pasteDelegate didTapSearchCopiedText];
}

#pragma mark - UIScribbleInteractionDelegate

- (void)scribbleInteractionWillBeginWriting:
    (UIScribbleInteraction*)interaction {
  if (self.textField.isPreEditing) {
    [self.textField exitPreEditState];
    [self.textField setText:[[NSAttributedString alloc] initWithString:@""]
             userTextLength:0];
  }

  [self.textField clearAutocompleteText];
}

- (void)scribbleInteractionDidFinishWriting:
    (UIScribbleInteraction*)interaction {
  [self cleanupOmniboxAfterScribble];

  // Dismiss any inline autocomplete. The user expectation is to not have it.
  [self.textField clearAutocompleteText];

  if (IsRichAutocompletionEnabled() && _textChangeDelegate) {
    _textChangeDelegate->OnRemoveAdditionalText();
  }
}

/// Handles interaction with the thumbnail button. (tap or keyboard delete)
- (void)didTapThumbnailButton {
  if (!self.view.thumbnailButton.selected) {
    self.view.thumbnailButton.selected = YES;
  } else {
    if (_textChangeDelegate) {
      _textChangeDelegate->RemoveThumbnail();
      // Clear the selection once it's no longer needed. This prevents it from
      // reappearing unexpectedly as the user navigates back through previous
      // results.
      self.view.thumbnailButton.selected = NO;
    }
  }
}

/// Returns the placeholder text for the current state.
- (NSString*)placeholderText {
  if (!base::FeatureList::IsEnabled(kEnableLensOverlay)) {
    return l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  }

  if (self.view.thumbnailImage) {
    return l10n_util::GetNSString(IDS_IOS_OMNIBOX_PLACEHOLDER_IMAGE_SEARCH);
  } else if (self.isSearchOnlyUI) {
    return l10n_util::GetNSStringF(IDS_IOS_OMNIBOX_PLACEHOLDER_SEARCH_ONLY,
                                   self.searchProviderName);
  } else {
    return l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  }
}

@end

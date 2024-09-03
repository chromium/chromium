// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_ios_uiview.h"

#include "base/apple/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/input/web_input_event_builders_ios.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"

static void* kObservingContext = &kObservingContext;

@implementation CALayerFrameSinkProvider
- (ui::CALayerFrameSink*)frameSink {
  return nil;
}
@end

@interface BETextPosition : UITextPosition {
  CGRect rect_;
}
- (instancetype)initWithRect:(CGRect)rect;

@end

@implementation BETextPosition
- (instancetype)initWithRect:(CGRect)rect {
  rect_ = rect;
  return [self init];
}
- (CGRect)rect {
  return rect_;
}
@end

@interface BETextRange : UITextRange {
  CGRect start_;
  CGRect end_;
}
- (instancetype)initWithRegion:
    (const content::TextInputManager::SelectionRegion*)region;
@end

@implementation BETextRange

- (instancetype)initWithRegion:
    (const content::TextInputManager::SelectionRegion*)region {
  start_ = CGRectMake(region->anchor.edge_start_rounded().x(),
                      region->anchor.edge_start_rounded().y(), 1,
                      region->anchor.GetHeight());

  end_ = CGRectMake(region->focus.edge_start_rounded().x(),
                    region->focus.edge_start_rounded().y(), 1,
                    region->focus.GetHeight());
  return [self init];
}

- (BOOL)isEmpty {
  return NO;
}

- (UITextPosition*)start {
  return [[BETextPosition alloc] initWithRect:end_];
}
- (UITextPosition*)end {
  return [[BETextPosition alloc] initWithRect:end_];
}
@end

@interface BETextSelectionHandles : UITextSelectionRect
- (instancetype)initWithCGRect:(CGRect)rect atStart:(BOOL)start;
@end
@implementation BETextSelectionHandles {
  CGRect rect_;
  BOOL start_;
}
- (instancetype)initWithCGRect:(CGRect)rect atStart:(BOOL)start {
  rect_ = rect;
  start_ = start;
  return [self init];
}
- (NSWritingDirection)writingDirection {
  return NSWritingDirectionLeftToRight;
}
- (CGRect)rect {
  return rect_;
}
- (BOOL)containsStart {
  return start_;
}
- (BOOL)containsEnd {
  return !start_;
}
@end

@interface BlinkExtendedTextInputTraits : NSObject <BEExtendedTextInputTraits>
@property(nonatomic) UITextAutocapitalizationType autocapitalizationType;
@property(nonatomic) UITextAutocorrectionType autocorrectionType;
@property(nonatomic) UITextSpellCheckingType spellCheckingType;
@property(nonatomic) UITextSmartQuotesType smartQuotesType;
@property(nonatomic) UITextSmartDashesType smartDashesType;
@property(nonatomic) UITextInlinePredictionType inlinePredictionType;
@property(nonatomic) UIKeyboardType keyboardType;
@property(nonatomic) UIKeyboardAppearance keyboardAppearance;
@property(nonatomic) UIReturnKeyType returnKeyType;
@property(nonatomic, getter=isSecureTextEntry) BOOL secureTextEntry;
@property(nonatomic, getter=isSingleLineDocument) BOOL singleLineDocument;
@property(nonatomic, getter=isTypingAdaptationEnabled)
    BOOL typingAdaptationEnabled;
@property(nonatomic, copy) UITextContentType textContentType;
@property(nonatomic, copy) UITextInputPasswordRules* passwordRules;
@property(nonatomic) UITextSmartInsertDeleteType smartInsertDeleteType;
@property(nonatomic) BOOL enablesReturnKeyAutomatically;
@property(nonatomic, strong) UIColor* insertionPointColor;
@property(nonatomic, strong) UIColor* selectionHandleColor;
@property(nonatomic, strong) UIColor* selectionHighlightColor;
@end
@implementation BlinkExtendedTextInputTraits
- (instancetype)init {
  if (!(self = [super init])) {
    return nil;
  }
  self.typingAdaptationEnabled = YES;
  self.selectionHandleColor = [UIColor blueColor];
  return self;
}

@end

@implementation RenderWidgetUIView
@synthesize textInput = _textInput;

- (instancetype)initWithWidget:
    (base::WeakPtr<content::RenderWidgetHostViewIOS>)view {
  self = [self init];
  if (self) {
    _view = view;
    text_interaction_ = [[BETextInteraction alloc] init];
    [self addInteraction:text_interaction_];
    self.multipleTouchEnabled = YES;
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _textInput = [[RenderWidgetUIViewTextInput alloc] initWithWidget:view];
    [self addSubview:_textInput];
  }
  return self;
}

- (void)layoutSubviews {
  CHECK(_view);
  [super layoutSubviews];
  _view->UpdateScreenInfo();

  // TODO(dtapuska): This isn't correct, we need to figure out when the window
  // gains/loses focus.
  _view->SetActive(true);
}

- (ui::CALayerFrameSink*)frameSink {
  return _view.get();
}

- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (BOOL)becomeFirstResponder {
  CHECK(_view);
  BOOL result = [super becomeFirstResponder];
  if (result || _view->CanBecomeFirstResponderForTesting()) {
    _view->OnFirstResponderChanged();
  }
  return result;
}

- (BOOL)resignFirstResponder {
  BOOL result = [super resignFirstResponder];
  if (_view && (result || _view->CanResignFirstResponderForTesting())) {
    _view->OnFirstResponderChanged();
  }
  return result;
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  CHECK(_view);
  if (!_view->HasFocus()) {
    if ([self becomeFirstResponder]) {
      _view->OnFirstResponderChanged();
    }
  }
  for (UITouch* touch in touches) {
    blink::WebTouchEvent webTouchEvent = input::WebTouchEventBuilder::Build(
        blink::WebInputEvent::Type::kTouchStart, touch, event, self,
        _viewOffsetDuringTouchSequence);
    if (!_viewOffsetDuringTouchSequence) {
      _viewOffsetDuringTouchSequence =
          webTouchEvent.touches[0].PositionInWidget() -
          webTouchEvent.touches[0].PositionInScreen();
    }
    _view->OnTouchEvent(std::move(webTouchEvent));
  }
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  CHECK(_view);
  for (UITouch* touch in touches) {
    _view->OnTouchEvent(input::WebTouchEventBuilder::Build(
        blink::WebInputEvent::Type::kTouchEnd, touch, event, self,
        _viewOffsetDuringTouchSequence));
  }
  if (event.allTouches.count == 1) {
    _viewOffsetDuringTouchSequence.reset();
  }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  CHECK(_view);
  for (UITouch* touch in touches) {
    _view->OnTouchEvent(input::WebTouchEventBuilder::Build(
        blink::WebInputEvent::Type::kTouchMove, touch, event, self,
        _viewOffsetDuringTouchSequence));
  }
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  CHECK(_view);
  for (UITouch* touch in touches) {
    _view->OnTouchEvent(input::WebTouchEventBuilder::Build(
        blink::WebInputEvent::Type::kTouchCancel, touch, event, self,
        _viewOffsetDuringTouchSequence));
  }
  _viewOffsetDuringTouchSequence.reset();
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  CHECK(_view);
  if (context == kObservingContext) {
    _view->ContentInsetChanged();
  } else {
    [super observeValueForKeyPath:keyPath
                         ofObject:object
                           change:change
                          context:context];
  }
}

- (void)removeView {
  UIScrollView* view = (UIScrollView*)[self superview];
  [view removeObserver:self
            forKeyPath:NSStringFromSelector(@selector(contentInset))];
  [self removeFromSuperview];
}

- (BETextInteraction*)textInteraction {
  return text_interaction_;
}

- (void)updateView:(UIScrollView*)view {
  [view addSubview:self];
  view.scrollEnabled = NO;
  // Remove all existing gestureRecognizers since the header might be reused.
  for (UIGestureRecognizer* recognizer in view.gestureRecognizers) {
    [view removeGestureRecognizer:recognizer];
  }
  [view addObserver:self
         forKeyPath:NSStringFromSelector(@selector(contentInset))
            options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld
            context:kObservingContext];
}

- (BOOL)isEditable {
  // Reflects the ability to modify text, but since Blink handles editing
  // it's unclear what this does. For now, always set this to NO.
  return NO;
}

- (BOOL)automaticallyPresentEditMenu {
  // Needs an edit menu implementation.
  return NO;
}

- (BOOL)isReplaceAllowed {
  // Needs an implementatino to check if the focused field allows replacements,
  // e.g. password fields do not.
  return NO;
}

- (BOOL)isSelectionAtDocumentStart {
  // Unclear what this does if true.
  return NO;
}

- (NSAttributedString*)attributedMarkedText {
  return nil;
}

- (CGRect)textFirstRect {
  // The bounds of the first line of either marked text or insertion point.
  return CGRectNull;
}

- (CGRect)textLastRect {
  // The bounds of the last line of either marked text or insertion point.
  return CGRectNull;
}

- (CGRect)unobscuredContentRect {
  // Similar to selectionClipRect, this needs to be larger or selection handles
  // will appear in the wrong place when zoomed out of view. This needs a proper
  // implementation showing the real rect of the view transformed from the
  // [view bounds]
  return CGRectMake(-1000, -1000, 10000, 10000);
}

- (UIView*)unscaledView {
  // View representing the web content that is agnostic of zoom state, so
  // returning self is a simple hack and wrong.
  return self;
}

- (id<BETextInputDelegate>)asyncInputDelegate {
  return be_text_input_delegate_;
}

- (id<UITextInputDelegate>)inputDelegate {
  return nil;
}

- (void)setInputDelegate:(id<UITextInputDelegate>)inputDelegate {
}

- (void)setAsyncInputDelegate:(id<BETextInputDelegate>)delegate {
  be_text_input_delegate_ = delegate;
}

- (UIView*)textInputView {
  return self;
}

- (BOOL)hasMarkedText {
  return NO;
}

- (NSString*)markedText {
  return nil;
}

- (NSString*)selectedText {
  if (!_view || !_view->GetTextInputManager()) {
    return nil;
  }
  const content::TextInputManager::TextSelection* selection =
      _view->GetTextInputManager()->GetTextSelection(_view.get());
  if (!selection || !selection->selected_text().length()) {
    return nil;
  }

  return base::SysUTF16ToNSString(selection->selected_text());
}

- (void)unmarkText {
}

- (CGRect)selectionClipRect {
  if (!_view || !_view->GetTextInputManager()) {
    return CGRectNull;
  }
  const content::TextInputManager::SelectionRegion* region =
      _view->GetTextInputManager()->GetSelectionRegion(_view.get());
  if (!region || !region->focus.HasHandle()) {
    return CGRectNull;
  }

  // Need to get a more realistic rect here. If this clip is too small,
  // selection handles won't draw correctly.
  return CGRectMake(-1000, -1000, 10000, 10000);
}

- (id<BEExtendedTextInputTraits>)extendedTextInputTraits {
  return [[BlinkExtendedTextInputTraits alloc] init];
}

- (void)moveInLayoutDirection:(UITextLayoutDirection)direction {
}

- (void)extendInLayoutDirection:(UITextLayoutDirection)direction {
}

- (void)moveInStorageDirection:(UITextStorageDirection)direction
                 byGranularity:(UITextGranularity)granularity {
}

- (void)extendInStorageDirection:(UITextStorageDirection)direction
                   byGranularity:(UITextGranularity)granularity {
}

- (BOOL)canPerformAction:(SEL)action withSender:(nullable id)sender {
  return YES;
}

- (void)handleKeyEntry:(BEKeyEntry*)entry
    withCompletionHandler:
        (void (^)(BEKeyEntry* theEvent, BOOL wasHandled))completionHandler {
}

- (void)shiftKeyStateChangedFromState:(BEKeyModifierFlags)oldState
                              toState:(BEKeyModifierFlags)newState {
}

- (void)deleteInDirection:(UITextStorageDirection)direction
            toGranularity:(UITextGranularity)granularity {
}

- (void)transposeCharactersAroundSelection {
}

- (void)replaceText:(NSString*)originalText
             withText:(NSString*)replacementText
              options:(BETextReplacementOptions)options
    completionHandler:
        (void (^)(NSArray<UITextSelectionRect*>* rects))completionHandler {
}

- (void)requestTextContextForAutocorrectionWithCompletionHandler:
    (void (^)(BETextDocumentContext* context))completionHandler {
  completionHandler(nil);
}

- (void)requestTextRectsForString:(NSString*)input
            withCompletionHandler:
                (void (^)(NSArray<UITextSelectionRect*>* rects))
                    completionHandler {
}

- (void)requestPreferredArrowDirectionForEditMenuWithCompletionHandler:
    (void (^)(UIEditMenuArrowDirection))completionHandler {
  completionHandler(UIEditMenuArrowDirectionAutomatic);
}

- (void)systemWillPresentEditMenuWithAnimator:
    (id<UIEditMenuInteractionAnimating>)animator
    API_UNAVAILABLE(watchos, tvos) {
}

- (void)systemWillDismissEditMenuWithAnimator:
    (id<UIEditMenuInteractionAnimating>)animator
    API_UNAVAILABLE(watchos, tvos) {
}

- (nullable NSDictionary<NSAttributedStringKey, id>*)
    textStylingAtPosition:(UITextPosition*)position
              inDirection:(UITextStorageDirection)direction {
  return nil;
}

- (void)replaceSelectedText:(NSString*)text
                   withText:(NSString*)replacementText {
}

- (void)updateCurrentSelectionTo:(CGPoint)point
                     fromGesture:(BEGestureType)gestureType
                         inState:(UIGestureRecognizerState)state {
  if (!_view) {
    return;
  }
  _view->host()->delegate()->MoveRangeSelectionExtent(
      gfx::Point(point.x, point.y));
}

- (void)setSelectionFromPoint:(CGPoint)from
                      toPoint:(CGPoint)to
                      gesture:(BEGestureType)gesture
                        state:(UIGestureRecognizerState)state
    NS_SWIFT_NAME(setSelection(from:to:gesture:state:)) {
}

- (void)adjustSelectionBoundaryToPoint:(CGPoint)point
                            touchPhase:(BESelectionTouchPhase)touch
                           baseIsStart:(BOOL)boundaryIsStart
                                 flags:(BESelectionFlags)flags {
  if (!_view || !_view->GetTextInputManager()) {
    return;
  }
  const content::TextInputManager::SelectionRegion* region =
      _view->GetTextInputManager()->GetSelectionRegion(_view.get());
  if (!region || !region->focus.HasHandle()) {
    return;
  }

  // A simple naive implementation that updates the selection range based on
  // a combination of boundaryIsStart (to know which handle was grabbed) and
  // SelectionRegion data. In the future this could be simplified with more
  // data, such as document position of selection to know which should be
  // start and end.
  CGPoint start, end;
  if (region->focus.type() == gfx::SelectionBound::RIGHT) {
    start = CGPointMake(region->focus.edge_start_rounded().x(),
                        region->focus.edge_start_rounded().y());
    end = CGPointMake(region->anchor.edge_start_rounded().x(),
                      region->anchor.edge_start_rounded().y());
  } else {
    end = CGPointMake(region->focus.edge_start_rounded().x(),
                      region->focus.edge_start_rounded().y());
    start = CGPointMake(region->anchor.edge_start_rounded().x(),
                        region->anchor.edge_start_rounded().y());
  }

  if (boundaryIsStart) {
    end = point;
  } else {
    start = point;
  }

  // This should look at document position instead, but for a naive
  // implementation works well enough.
  if (end.x < start.x && end.y < start.y) {
    flags = BESelectionFlipped;
    CGPoint flip = start;
    start = end;
    end = flip;
  }

  _view->host()->delegate()->SelectRange(gfx::Point(start.x, start.y),
                                         gfx::Point(end.x, end.y));

  // Tells the system the selection adjustment has been handled for the given
  // `point` and touch.
  [text_interaction_ selectionBoundaryAdjustedToPoint:point
                                           touchPhase:touch
                                                flags:flags];
}

- (BOOL)textInteractionGesture:(BEGestureType)gestureType
            shouldBeginAtPoint:(CGPoint)point {
  // Check if point is really selectable here.
  return NO;
}

- (void)selectWordForReplacement {
}

- (void)updateSelectionWithExtentPoint:(CGPoint)point
                              boundary:(UITextGranularity)granularity
                     completionHandler:(void (^)(BOOL selectionEndIsMoving))
                                           completionHandler {
  if (!_view) {
    return;
  }
  _view->host()->delegate()->MoveRangeSelectionExtent(
      gfx::Point(point.x, point.y));
  completionHandler(true);
}

- (void)selectTextInGranularity:(UITextGranularity)granularity
                        atPoint:(CGPoint)point
              completionHandler:(void (^)(void))completionHandler {
  if (!_view) {
    completionHandler();
    return;
  }
  _view->host()->delegate()->MoveCaret(gfx::Point(point.x, point.y));
  _view->host()->delegate()->SelectRange(gfx::Point(point.x, point.y),
                                         gfx::Point(point.x, point.y));
  _view->host()->delegate()->SelectRange(gfx::Point(point.x, point.y),
                                         gfx::Point(point.x, point.y));
  _view->host()->delegate()->SelectAroundCaret(
      blink::mojom::SelectionGranularity::kWord,
      /*should_show_handle=*/true,
      /*should_show_context_menu=*/false);
  completionHandler();
}

- (void)selectPositionAtPoint:(CGPoint)point
            completionHandler:(void (^)(void))completionHandler {
  // Unclear when this is used instead of selectTextInGranularity.
  [self selectTextInGranularity:UITextGranularityWord
                        atPoint:point
              completionHandler:completionHandler];
}

- (void)selectPositionAtPoint:(CGPoint)point
           withContextRequest:(BETextDocumentRequest*)request
            completionHandler:
                (void (^)(BETextDocumentContext*))completionHandler {
}

- (void)adjustSelectionByRange:(BEDirectionalTextRange)range
             completionHandler:(void (^)(void))completionHandler {
}

- (void)moveByOffset:(NSInteger)offset {
}

- (void)moveSelectionAtBoundary:(UITextGranularity)granularity
             inStorageDirection:(UITextStorageDirection)direction
              completionHandler:(void (^)(void))completionHandler {
}

- (void)
    selectTextForEditMenuWithLocationInView:(CGPoint)locationInView
                          completionHandler:
                              (void (^)(BOOL shouldPresentMenu,
                                        NSString* _Nullable contextString,
                                        NSRange selectedRangeInContextString))
                                  completionHandler {
}

- (void)setAttributedMarkedText:(nullable NSAttributedString*)markedText
                  selectedRange:(NSRange)selectedRange {
}

- (BOOL)isPointNearMarkedText:(CGPoint)point {
  // This needs a real implementation.
  return YES;
}

- (void)requestDocumentContext:(BETextDocumentRequest*)request
             completionHandler:
                 (void (^)(BETextDocumentContext*))completionHandler {
  completionHandler(nil);
}

- (void)willInsertFinalDictationResult {
}

- (void)replaceDictatedText:(NSString*)oldText withText:(NSString*)newText {
}

- (void)didInsertFinalDictationResult {
}

- (nullable NSArray<BETextAlternatives*>*)alternativesForSelectedText {
  return nil;
}

- (void)addTextAlternatives:(BETextAlternatives*)alternatives {
}

- (void)insertTextAlternatives:(BETextAlternatives*)alternatives {
}

- (void)insertTextPlaceholderWithSize:(CGSize)size
                    completionHandler:
                        (void (^)(UITextPlaceholder*))completionHandler {
}

- (void)removeTextPlaceholder:(UITextPlaceholder*)placeholder
               willInsertText:(BOOL)willInsertText
            completionHandler:(void (^)(void))completionHandler {
}

- (void)insertTextSuggestion:(BETextSuggestion*)textSuggestion {
}

- (void)autoscrollToPoint:(CGPoint)point {
  // This is a good place to tell Blink to auto scroll.
}

- (void)cancelAutoscroll {
  // This is a good place to tell Blink to stop auto scroll.
}

- (UITextRange*)markedTextRange {
  return nil;
}

- (NSDictionary*)markedTextStyle {
  return nil;
}

- (void)setMarkedTextStyle:(NSDictionary*)styleDictionary {
}

- (UITextPosition*)beginningOfDocument {
  return nil;
}

- (UITextPosition*)endOfDocument {
  return nil;
}

- (BOOL)hasText {
  return YES;
}

- (void)insertText:(NSString*)text {
}

- (void)deleteBackward {
}

- (void)setSelectedTextRange:(UITextRange*)range {
}

- (UITextRange*)selectedTextRange {
  if (!_view || !_view->GetTextInputManager()) {
    return nil;
  }
  const content::TextInputManager::SelectionRegion* region =
      _view->GetTextInputManager()->GetSelectionRegion(_view.get());
  if (region) {
    return [[BETextRange alloc] initWithRegion:region];
  }

  return nil;
}
- (nullable NSString*)textInRange:(UITextRange*)range {
  return nil;
}

- (void)replaceRange:(UITextRange*)range withText:(NSString*)text {
}

- (void)setMarkedText:(nullable NSString*)markedText
        selectedRange:(NSRange)selectedRange {
}

- (nullable UITextRange*)textRangeFromPosition:(UITextPosition*)fromPosition
                                    toPosition:(UITextPosition*)toPosition {
  return nil;
}

- (nullable UITextPosition*)positionFromPosition:(UITextPosition*)position
                                          offset:(NSInteger)offset {
  return nil;
}

- (nullable UITextPosition*)positionFromPosition:(UITextPosition*)position
                                     inDirection:
                                         (UITextLayoutDirection)direction
                                          offset:(NSInteger)offset {
  return nil;
}

- (NSComparisonResult)comparePosition:(UITextPosition*)position
                           toPosition:(UITextPosition*)other {
  return NSOrderedSame;
}

- (NSInteger)offsetFromPosition:(UITextPosition*)from
                     toPosition:(UITextPosition*)toPosition {
  return 0;
}

- (nullable UITextPosition*)positionWithinRange:(UITextRange*)range
                            farthestInDirection:
                                (UITextLayoutDirection)direction {
  return nil;
}

- (nullable UITextRange*)
    characterRangeByExtendingPosition:(UITextPosition*)position
                          inDirection:(UITextLayoutDirection)direction {
  return nil;
}

- (NSWritingDirection)baseWritingDirectionForPosition:(UITextPosition*)position
                                          inDirection:(UITextStorageDirection)
                                                          direction {
  return NSWritingDirectionNatural;
}

- (void)setBaseWritingDirection:(NSWritingDirection)writingDirection
                       forRange:(UITextRange*)range {
}

- (CGRect)caretRectForPosition:(UITextPosition*)position {
  BETextPosition* be_position = base::apple::ObjCCast<BETextPosition>(position);
  if (be_position) {
    return [be_position rect];
  }
  return CGRectNull;
}

- (NSArray<UITextSelectionRect*>*)selectionRectsForRange:(UITextRange*)range {
  if (!_view || !_view->GetTextInputManager()) {
    return @[];
  }
  // The following should instead use |range| rather than assuming
  // GetSelectionRegion. Consider this proof-of-concept only.
  const content::TextInputManager::SelectionRegion* region =
      _view->GetTextInputManager()->GetSelectionRegion(_view.get());
  if (!region || !region->focus.HasHandle() ||
      region->focus.type() == gfx::SelectionBound::CENTER) {
    return @[];
  }

  UITextSelectionRect* start = [[BETextSelectionHandles alloc]
      initWithCGRect:CGRectMake(region->focus.edge_start_rounded().x(),
                                region->focus.edge_start_rounded().y(), 1,
                                region->focus.GetHeight())
             atStart:region->focus.type() == gfx::SelectionBound::RIGHT];
  UITextSelectionRect* end = [[BETextSelectionHandles alloc]
      initWithCGRect:CGRectMake(region->anchor.edge_start_rounded().x(),
                                region->anchor.edge_start_rounded().y(), 1,
                                region->anchor.GetHeight())
             atStart:region->anchor.type() == gfx::SelectionBound::RIGHT];
  return @[ start, end ];
}

#pragma mark - Hit testing

- (nullable UITextPosition*)closestPositionToPoint:(CGPoint)point {
  return nil;
}

- (nullable UITextPosition*)closestPositionToPoint:(CGPoint)point
                                       withinRange:(UITextRange*)range {
  return nil;
}

- (nullable UITextRange*)characterRangeAtPoint:(CGPoint)point {
  return nil;
}

- (NSArray*)accessibilityElements {
  ui::BrowserAccessibilityManager* manager =
      _view->host()->GetRootBrowserAccessibilityManager();
  if (manager) {
    id root = manager->GetBrowserAccessibilityRoot()->GetNativeViewAccessible();
    if (root) {
      return @[ root ];
    }
  }
  return nil;
}

- (BOOL)isAccessibilityElement {
  return NO;
}

@end

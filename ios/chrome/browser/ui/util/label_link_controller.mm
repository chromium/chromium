// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/label_link_controller.h"

#include <map>
#include <vector>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/ui/util/CRUILabel+AttributeUtils.h"
#import "ios/chrome/browser/ui/util/label_observer.h"
#import "ios/chrome/browser/ui/util/text_region_mapper.h"
#import "ios/chrome/browser/ui/util/transparent_link_button.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - LinkData

// Object encapsulating the range of a link and the frames corresponding with
// that range.
@interface LinkData : NSObject

// Designated initializer.
- (instancetype)initWithRange:(NSRange)range NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The range passed on initialization.
@property(nonatomic, readonly) NSRange range;

// The frames calculated for |_range|.
@property(nonatomic, strong) NSArray* frames;

// Accessibility identifier for the link button.
@property(nonatomic, copy) NSString* accessibilityID;

@end

@implementation LinkData

- (instancetype)initWithRange:(NSRange)range {
  if ((self = [super init])) {
    DCHECK_NE(range.location, static_cast<NSUInteger>(NSNotFound));
    DCHECK_NE(range.length, 0U);
    _range = range;
  }
  return self;
}

@end

#pragma mark - LabelLinkController

@interface LabelLinkController ()
// Private property exposed publically in testing interface.
@property(nonatomic, weak) Class textMapperClass;

// The original attributed text set on the label.  This may be different from
// the label's |attributedText| property, as additional style attributes may be
// introduced for links.
@property(nonatomic, strong, readonly) NSAttributedString* originalLabelText;

// The array of TransparentLinkButtons inserted above the label.
@property(nonatomic, strong, readonly) NSMutableArray* linkButtons;

// Adds LabelObserverActions to the LabelObserver corresponding to |_label|.
- (void)addLabelObserverActions;

// Clears all defined links and any data associated with them. Update the
// original attributed text from the controlled label.
- (void)reset;

// Handle a change to the label that changes the positioning of glyphs but not
// any styling of those glyphs. Forces a recomputation of the tap regions, and
// recreates any tap buttons.
- (void)labelLayoutInvalidated;

// Handle a change to the label that changes the glyph style. This forces all of
// the link-specific styling applied by this class to be regenerated (which
// itself will again re-trigger this method), and because any kind of style
// change may alter the position of glyphs, this forces a layout invalidation.
- (void)labelStyleInvalidated;

// Updates the attributed string content of the controlled label to
// have the designated link colors and styles.
// No-op if no links are defined.
- (void)updateStyles;

// If the controlled label's bounds have changed from the last time tap rects
// were updated, determine which regions in the label should be tappable.
- (void)updateTapRects;

// Creates a new text mapper instance with the current label bounds and
// attributed text.
- (void)resetTextMapper;

// Clear any tap buttons that have been created, removing them from their
// superview if necessary.
- (void)clearTapButtons;

// Updates the tap buttons as detailed below. This method is called every time
// tap rects are updated, as well as every time |_label|'s superview changes.
// If there are no tap buttons defined, but there are known tap rects, and
// |_label| has a superview, then tap buttons are created and added to that
// view.
// If there are tap buttons, and |_label| has no superview, then the tap buttons
// are cleared.
// If there are tap buttons, but they are not subviews of |_label|'s superview
// (if _label's superview has changed since the buttons were created), then
// the tap buttons are migrated into the new superview.
- (void)updateTapButtons;

@end

@implementation LabelLinkController {
  // Ivars immutable for the lifetime of the object.
  ProceduralBlockWithURL _action;
  UILabel* _label;
  UITapGestureRecognizer* _linkTapRecognizer;

  // Ivars that reset when label text changes.
  NSMutableDictionary* _linkDataForURLs;
  CGRect _lastLabelFrame;

  // Ivars that reset when text or bounds change.
  id<TextRegionMapper> _textMapper;

  // Internal tracking.
  BOOL _justUpdatedStyles;
  LabelObserver* _labelObserver;
}

@synthesize showTapAreas = _showTapAreas;
@synthesize textMapperClass = _textMapperClass;
@synthesize linkUnderlineStyle = _linkUnderlineStyle;
@synthesize linkButtons = _linkButtons;
@synthesize originalLabelText = _originalLabelText;
@synthesize linkFont = _linkFont;
@synthesize linkColor = _linkColor;

- (instancetype)initWithLabel:(UILabel*)label
                       action:(ProceduralBlockWithURL)action {
  if ((self = [super init])) {
    DCHECK(label);
    _label = label;
    _action = [action copy];
    _linkUnderlineStyle = NSUnderlineStyleNone;
    [self reset];

    _labelObserver = [LabelObserver observerForLabel:_label];
    [_labelObserver startObserving];
    [self addLabelObserverActions];

    self.textMapperClass = [CoreTextRegionMapper class];
    _linkButtons = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)addLabelObserverActions {
  __weak LabelLinkController* weakSelf = self;
  [_labelObserver addStyleChangedAction:^(UILabel* label) {
    // One of the style properties has been changed, which will silently
    // update the label's attributedText.
    if (!weakSelf)
      return;
    LabelLinkController* strongSelf = weakSelf;
    [strongSelf labelStyleInvalidated];
  }];
  [_labelObserver addTextChangedAction:^(UILabel* label) {
    if (!weakSelf)
      return;
    LabelLinkController* strongSelf = weakSelf;
    NSString* originalText = [[strongSelf originalLabelText] string];
    if ([label.text isEqualToString:originalText]) {
      // The actual text of the label didn't change, so this was a change to
      // the string attributes only.
      [strongSelf labelStyleInvalidated];
    } else {
      // The label text has changed, so start everything from scratch.
      [strongSelf reset];
    }
  }];
  [_labelObserver addLayoutChangedAction:^(UILabel* label) {
    if (!weakSelf)
      return;
    LabelLinkController* strongSelf = weakSelf;
    [strongSelf labelLayoutInvalidated];
    NSArray* linkButtons = [strongSelf linkButtons];
    // If this layout change corresponds to |label|'s moving to a new superview,
    // update the tap buttons so that they are inserted above |label| in the new
    // hierarchy.
    if (linkButtons.count && label.superview != [linkButtons[0] superview])
      [strongSelf updateTapButtons];
  }];
}

- (void)dealloc {
  [self clearTapButtons];
  [_labelObserver stopObserving];
}

- (void)addLinkWithRange:(NSRange)range url:(GURL)url {
  [self addLinkWithRange:range url:url accessibilityID:nil];
}

- (void)addLinkWithRange:(NSRange)range
                     url:(GURL)url
         accessibilityID:(NSString*)accessibilityID {
  DCHECK(url.is_valid());
  if (!_linkDataForURLs)
    _linkDataForURLs = [[NSMutableDictionary alloc] init];
  NSURL* key = net::NSURLWithGURL(url);
  LinkData* linkData = [[LinkData alloc] initWithRange:range];
  linkData.accessibilityID = accessibilityID;
  [_linkDataForURLs setObject:linkData forKey:key];
  [self updateStyles];
}

- (void)setLinkColor:(UIColor*)linkColor {
  _linkColor = [linkColor copy];
  [self updateStyles];
}

- (void)setLinkUnderlineStyle:(NSUnderlineStyle)underlineStyle {
  _linkUnderlineStyle = underlineStyle;
  [self updateStyles];
}

- (void)setLinkFont:(UIFont*)linkFont {
  _linkFont = linkFont;
  [self updateStyles];
}

- (void)setShowTapAreas:(BOOL)showTapAreas {
#ifndef NDEBUG
  for (TransparentLinkButton* button in _linkButtons) {
    button.debug = showTapAreas;
  }
#endif  // NDEBUG
  _showTapAreas = showTapAreas;
}

#pragma mark - internal methods

- (void)reset {
  _originalLabelText = [[_label attributedText] copy];
  _textMapper = nil;
  _lastLabelFrame = CGRectZero;
  _linkDataForURLs = nil;
}

- (void)labelLayoutInvalidated {
  _textMapper = nil;
  [self updateTapRects];
}

- (void)labelStyleInvalidated {
  // If the style invalidation was triggered by this class updating link styles,
  // then the original label text is still correct, but the tap rects still need
  // to be updated. Otherwise, update the original label text, and then update
  // styles. This will set |_justUpdatedStyles| and trigger another call to
  // this method via KVO.
  if (_justUpdatedStyles) {
    // TODO(crbug.com/664648): Remove _justUpdatedStyles due to bug that
    // prevents proper style updates after successive label format changes.
    _justUpdatedStyles = NO;
  } else if (![_originalLabelText isEqual:[_label attributedText]]) {
    _originalLabelText = [[_label attributedText] copy];
    [self updateStyles];
  }
  _lastLabelFrame = CGRectZero;
  [self labelLayoutInvalidated];
}

- (void)updateStyles {
  if (![_linkDataForURLs count])
    return;

  __block NSMutableAttributedString* labelText =
      [_originalLabelText mutableCopy];
  [_linkDataForURLs enumerateKeysAndObjectsUsingBlock:^(
                        NSURL* key, LinkData* linkData, BOOL* stop) {
    if (_linkColor) {
      [labelText addAttribute:NSForegroundColorAttributeName
                        value:_linkColor
                        range:linkData.range];
    }
    if (_linkUnderlineStyle != NSUnderlineStyleNone) {
      [labelText addAttribute:NSUnderlineStyleAttributeName
                        value:@(_linkUnderlineStyle)
                        range:linkData.range];
    }
    if (_linkFont) {
      [labelText addAttribute:NSFontAttributeName
                        value:_linkFont
                        range:linkData.range];
    }
  }];
  _justUpdatedStyles = YES;
  [_label setAttributedText:labelText];
  _textMapper = nil;
}

- (void)updateTapRects {
  // Don't update if the label hasn't changed size or position.
  if (CGRectEqualToRect([_label frame], _lastLabelFrame))
    return;
  // Don't update if there are no links.
  if (![_linkDataForURLs count])
    return;

  _lastLabelFrame = [_label frame];
  [self clearTapButtons];

  // If the label bounds are zero in either dimension, no rects are possible.
  if (0.0 == _lastLabelFrame.size.width || 0.0 == _lastLabelFrame.size.height)
    return;

  if (!_textMapper)
    [self resetTextMapper];

  for (LinkData* linkData in [_linkDataForURLs allValues]) {
    NSMutableArray* frames = [[NSMutableArray alloc] init];
    NSArray* rects = [_textMapper rectsForRange:linkData.range];
    for (NSUInteger rectIdx = 0; rectIdx < [rects count]; ++rectIdx) {
      CGRect frame = [rects[rectIdx] CGRectValue];
      frame = [[_label superview] convertRect:frame fromView:_label];
      [frames addObject:[NSValue valueWithCGRect:frame]];
    }
    linkData.frames = frames;
  }
  [self updateTapButtons];
}

- (void)resetTextMapper {
  DCHECK([self.textMapperClass conformsToProtocol:@protocol(TextRegionMapper)]);
  _textMapper = [[self.textMapperClass alloc]
      initWithAttributedString:[_label attributedText]
                        bounds:[_label bounds]];
}

- (void)clearTapButtons {
  for (TransparentLinkButton* button in _linkButtons) {
    [button removeFromSuperview];
  }
  [_linkButtons removeAllObjects];
}

- (void)updateTapButtons {
  // If the label has no superview, clear any existing buttons.
  if (![_label superview]) {
    [self clearTapButtons];
    return;
  } else if ([_linkButtons count]) {
    // If the buttons are currently in some view other than the label's
    // superview, repatriate them.
    if (base::mac::ObjCCast<TransparentLinkButton>(_linkButtons[0]).superview !=
        [_label superview]) {
      for (TransparentLinkButton* button in _linkButtons) {
        CGRect newFrame =
            [[_label superview] convertRect:button.frame fromView:button];
        [[_label superview] insertSubview:button aboveSubview:_label];
        button.frame = newFrame;
      }
    }
  }
  // If there are no buttons, make some and put them in the label's superview.
  if (![_linkButtons count] && _linkDataForURLs) {
    [_linkDataForURLs enumerateKeysAndObjectsUsingBlock:^(
                          NSURL* key, LinkData* linkData, BOOL* stop) {
      GURL URL = net::GURLWithNSURL(key);
      NSString* accessibilityLabel =
          [[_label text] substringWithRange:linkData.range];
      // Only pass along line height if there are more than one layouts that
      // can overlap.
      CGFloat lineHeightLimit =
          [_linkDataForURLs count] > 1 ? _label.cr_lineHeight : 0;
      NSArray* buttons =
          [TransparentLinkButton buttonsForLinkFrames:linkData.frames
                                                  URL:URL
                                           lineHeight:lineHeightLimit
                                   accessibilityLabel:accessibilityLabel
                                      accessibilityID:linkData.accessibilityID];
      for (TransparentLinkButton* button in buttons) {
#ifndef NDEBUG
        button.debug = self.showTapAreas;
#endif  // NDEBUG
        [button addTarget:self
                      action:@selector(linkButtonTapped:)
            forControlEvents:UIControlEventTouchUpInside];
        [[_label superview] insertSubview:button aboveSubview:_label];
        [_linkButtons addObject:button];
      }
    }];
  }
}

#pragma mark - Tap Handlers

- (void)linkButtonTapped:(id)sender {
  TransparentLinkButton* button =
      base::mac::ObjCCast<TransparentLinkButton>(sender);
  _action(button.URL);
}

#pragma mark - Test facilitators

- (NSArray*)tapRectsForURL:(GURL)url {
  NSURL* key = net::NSURLWithGURL(url);
  LinkData* linkData = [_linkDataForURLs objectForKey:key];
  return linkData.frames;
}

- (NSArray*)buttonFrames {
  NSMutableArray* array =
      [NSMutableArray arrayWithCapacity:[_linkButtons count]];
  for (TransparentLinkButton* button in _linkButtons) {
    [array addObject:[NSValue valueWithCGRect:button.frame]];
  }
  return array;
}

- (void)tapLabelAtPoint:(CGPoint)point {
  [_linkDataForURLs enumerateKeysAndObjectsUsingBlock:^(
                        NSURL* key, LinkData* linkData, BOOL* stop) {
    for (NSValue* frameValue in linkData.frames) {
      CGRect frame = [frameValue CGRectValue];
      if (CGRectContainsPoint(frame, point)) {
        _action(net::GURLWithNSURL(key));
        *stop = YES;
        break;
      }
    }
  }];
}

@end

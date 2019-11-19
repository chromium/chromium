/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Google, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#import "third_party/blink/renderer/core/layout/layout_theme_mac.h"

#import <AvailabilityMacros.h>
#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <math.h>

#import "base/mac/mac_util.h"
#import "third_party/blink/public/platform/mac/web_sandbox_support.h"
#import "third_party/blink/public/platform/platform.h"
#import "third_party/blink/public/resources/grit/blink_resources.h"
#import "third_party/blink/public/strings/grit/blink_strings.h"
#import "third_party/blink/renderer/core/css_value_keywords.h"
#import "third_party/blink/renderer/core/fileapi/file_list.h"
#import "third_party/blink/renderer/core/html_names.h"
#import "third_party/blink/renderer/core/layout/layout_progress.h"
#import "third_party/blink/renderer/core/layout/layout_theme_default.h"
#import "third_party/blink/renderer/core/layout/layout_view.h"
#import "third_party/blink/renderer/core/style/shadow_list.h"
#import "third_party/blink/renderer/platform/data_resource_helper.h"
#import "third_party/blink/renderer/platform/fonts/string_truncator.h"
#import "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#import "third_party/blink/renderer/platform/mac/block_exceptions.h"
#import "third_party/blink/renderer/platform/mac/color_mac.h"
#import "third_party/blink/renderer/platform/mac/web_core_ns_cell_extras.h"
#import "third_party/blink/renderer/platform/runtime_enabled_features.h"
#import "third_party/blink/renderer/platform/text/platform_locale.h"
#import "third_party/blink/renderer/platform/web_test_support.h"

// This is a view whose sole purpose is to tell AppKit that it's flipped.
@interface BlinkFlippedControl : NSControl
@end

@implementation BlinkFlippedControl

- (BOOL)isFlipped {
  return YES;
}

- (NSText*)currentEditor {
  return nil;
}

- (BOOL)_automaticFocusRingDisabled {
  return YES;
}

@end
// The methods in this file are specific to the Mac OS X platform.

@interface BlinkLayoutThemeNotificationObserver : NSObject {
  blink::LayoutTheme* _theme;
}

- (id)initWithTheme:(blink::LayoutTheme*)theme;
- (void)systemColorsDidChange:(NSNotification*)notification;

@end

@implementation BlinkLayoutThemeNotificationObserver

- (id)initWithTheme:(blink::LayoutTheme*)theme {
  if (!(self = [super init]))
    return nil;

  _theme = theme;
  return self;
}

- (void)systemColorsDidChange:(NSNotification*)unusedNotification {
  DCHECK([[unusedNotification name]
      isEqualToString:NSSystemColorsDidChangeNotification]);
  _theme->PlatformColorsDidChange();
}

@end

@interface NSTextFieldCell (WKDetails)
- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame
                                        inView:(NSView*)controlView
                                  includeFocus:(BOOL)includeFocus;
@end

@interface BlinkTextFieldCell : NSTextFieldCell
- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame
                                        inView:(NSView*)controlView
                                  includeFocus:(BOOL)includeFocus;
@end

@implementation BlinkTextFieldCell
- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame
                                        inView:(NSView*)controlView
                                  includeFocus:(BOOL)includeFocus {
  // FIXME: This is a post-Lion-only workaround for <rdar://problem/11385461>.
  // When that bug is resolved, we should remove this code.
  CFMutableDictionaryRef coreUIDrawOptions = CFDictionaryCreateMutableCopy(
      NULL, 0, [super _coreUIDrawOptionsWithFrame:cellFrame
                                           inView:controlView
                                     includeFocus:includeFocus]);
  CFDictionarySetValue(coreUIDrawOptions, @"borders only", kCFBooleanTrue);
  return (CFDictionaryRef)[NSMakeCollectable(coreUIDrawOptions) autorelease];
}
@end

@interface BlinkFlippedView : NSView
@end

@implementation BlinkFlippedView

- (BOOL)isFlipped {
  return YES;
}

- (NSText*)currentEditor {
  return nil;
}

@end

namespace blink {

namespace {

class LayoutThemeMacRefresh final : public LayoutThemeDefault {
 public:
  static scoped_refptr<LayoutTheme> Create() {
    return base::AdoptRef(new LayoutThemeMacRefresh());
  }
};

// Inflate an IntRect to account for specific padding around margins.
enum { kTopMargin = 0, kRightMargin = 1, kBottomMargin = 2, kLeftMargin = 3 };

bool FontSizeMatchesToControlSize(const ComputedStyle& style) {
  int font_size = style.FontSize();
  if (font_size == [NSFont systemFontSizeForControlSize:NSRegularControlSize])
    return true;
  if (font_size == [NSFont systemFontSizeForControlSize:NSSmallControlSize])
    return true;
  if (font_size == [NSFont systemFontSizeForControlSize:NSMiniControlSize])
    return true;
  return false;
}

Color GetSystemColor(MacSystemColorID color_id) {
  // In tests, a WebSandboxSupport may not be set up. Just return a dummy
  // color, in this case, black.
  auto* sandbox_support = Platform::Current()->GetSandboxSupport();
  if (!sandbox_support)
    return Color();
  return sandbox_support->GetSystemColor(color_id);
}

// Helper functions used by a bunch of different control parts.
NSControlSize ControlSizeForFont(const FontDescription& font_description) {
  int font_size = font_description.ComputedPixelSize();
  if (font_size >= 16)
    return NSRegularControlSize;
  if (font_size >= 11)
    return NSSmallControlSize;
  return NSMiniControlSize;
}

LengthSize SizeFromNSControlSize(NSControlSize ns_control_size,
                                 const LengthSize& zoomed_size,
                                 float zoom_factor,
                                 const IntSize* sizes) {
  IntSize control_size = sizes[ns_control_size];
  if (zoom_factor != 1.0f)
    control_size = IntSize(control_size.Width() * zoom_factor,
                           control_size.Height() * zoom_factor);
  LengthSize result = zoomed_size;
  if (zoomed_size.Width().IsIntrinsicOrAuto() && control_size.Width() > 0)
    result.SetWidth(Length::Fixed(control_size.Width()));
  if (zoomed_size.Height().IsIntrinsicOrAuto() && control_size.Height() > 0)
    result.SetHeight(Length::Fixed(control_size.Height()));
  return result;
}

LengthSize SizeFromFont(const FontDescription& font_description,
                        const LengthSize& zoomed_size,
                        float zoom_factor,
                        const IntSize* sizes) {
  return SizeFromNSControlSize(ControlSizeForFont(font_description),
                               zoomed_size, zoom_factor, sizes);
}

}  // namespace

LayoutThemeMac::LayoutThemeMac()
    : LayoutTheme(),
      notification_observer_(
          [[BlinkLayoutThemeNotificationObserver alloc] initWithTheme:this]),
      painter_(*this) {
  [[NSNotificationCenter defaultCenter]
      addObserver:notification_observer_
         selector:@selector(systemColorsDidChange:)
             name:NSSystemColorsDidChangeNotification
           object:nil];
}

LayoutThemeMac::~LayoutThemeMac() {
  [[NSNotificationCenter defaultCenter] removeObserver:notification_observer_];
}

Color LayoutThemeMac::PlatformActiveSelectionBackgroundColor(
    WebColorScheme color_scheme) const {
  return GetSystemColor(MacSystemColorID::kSelectedTextBackground);
}

Color LayoutThemeMac::PlatformInactiveSelectionBackgroundColor(
    WebColorScheme color_scheme) const {
  return GetSystemColor(MacSystemColorID::kSecondarySelectedControl);
}

Color LayoutThemeMac::PlatformActiveSelectionForegroundColor(
    WebColorScheme color_scheme) const {
  return Color::kBlack;
}

Color LayoutThemeMac::PlatformActiveListBoxSelectionBackgroundColor(
    WebColorScheme color_scheme) const {
  return GetSystemColor(MacSystemColorID::kAlternateSelectedControl);
}

Color LayoutThemeMac::PlatformActiveListBoxSelectionForegroundColor(
    WebColorScheme color_scheme) const {
  return Color::kWhite;
}

Color LayoutThemeMac::PlatformInactiveListBoxSelectionForegroundColor(
    WebColorScheme color_scheme) const {
  return Color::kBlack;
}

Color LayoutThemeMac::PlatformSpellingMarkerUnderlineColor() const {
  return Color(251, 45, 29);
}

Color LayoutThemeMac::PlatformGrammarMarkerUnderlineColor() const {
  return Color(107, 107, 107);
}

Color LayoutThemeMac::PlatformFocusRingColor() const {
  static const RGBA32 kOldAquaFocusRingColor = 0xFF7DADD9;
  if (UsesTestModeFocusRingColor())
    return kOldAquaFocusRingColor;

  // TODO(crbug.com/929098) Need to pass an appropriate color scheme here.
  return SystemColor(CSSValueID::kWebkitFocusRingColor,
                     ComputedStyle::InitialStyle().UsedColorScheme());
}

Color LayoutThemeMac::PlatformInactiveListBoxSelectionBackgroundColor(
    WebColorScheme color_scheme) const {
  return PlatformInactiveSelectionBackgroundColor(color_scheme);
}

static FontSelectionValue ToFontWeight(NSInteger app_kit_font_weight) {
  DCHECK_GT(app_kit_font_weight, 0);
  DCHECK_LT(app_kit_font_weight, 15);
  if (app_kit_font_weight > 14)
    app_kit_font_weight = 14;
  else if (app_kit_font_weight < 1)
    app_kit_font_weight = 1;

  static FontSelectionValue font_weights[] = {
      FontSelectionValue(100), FontSelectionValue(100), FontSelectionValue(200),
      FontSelectionValue(300), FontSelectionValue(400), FontSelectionValue(500),
      FontSelectionValue(600), FontSelectionValue(600), FontSelectionValue(700),
      FontSelectionValue(800), FontSelectionValue(800), FontSelectionValue(900),
      FontSelectionValue(900), FontSelectionValue(900)};
  return font_weights[app_kit_font_weight - 1];
}

static inline NSFont* SystemNSFont(CSSValueID system_font_id) {
  switch (system_font_id) {
    case CSSValueID::kSmallCaption:
      return [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
    case CSSValueID::kMenu:
      return [NSFont menuFontOfSize:[NSFont systemFontSize]];
    case CSSValueID::kStatusBar:
      return [NSFont labelFontOfSize:[NSFont labelFontSize]];
    case CSSValueID::kWebkitMiniControl:
      return [NSFont
          systemFontOfSize:[NSFont
                               systemFontSizeForControlSize:NSMiniControlSize]];
    case CSSValueID::kWebkitSmallControl:
      return [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:
                                                  NSSmallControlSize]];
    case CSSValueID::kWebkitControl:
      return [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:
                                                  NSRegularControlSize]];
    default:
      return [NSFont systemFontOfSize:[NSFont systemFontSize]];
  }
}

void LayoutThemeMac::SystemFont(CSSValueID system_font_id,
                                FontSelectionValue& font_slope,
                                FontSelectionValue& font_weight,
                                float& font_size,
                                AtomicString& font_family) const {
  NSFont* font = SystemNSFont(system_font_id);
  if (!font)
    return;

  NSFontManager* font_manager = [NSFontManager sharedFontManager];
  font_slope = ([font_manager traitsOfFont:font] & NSItalicFontMask)
                   ? ItalicSlopeValue()
                   : NormalSlopeValue();
  font_weight = ToFontWeight([font_manager weightOfFont:font]);
  font_size = [font pointSize];
  font_family = font_family_names::kSystemUi;
}

void LayoutThemeMac::PlatformColorsDidChange() {
  system_color_cache_.clear();
  LayoutTheme::PlatformColorsDidChange();
}

Color LayoutThemeMac::SystemColor(CSSValueID css_value_id,
                                  WebColorScheme color_scheme) const {
  {
    HashMap<CSSValueID, RGBA32>::iterator it =
        system_color_cache_.find(css_value_id);
    if (it != system_color_cache_.end())
      return it->value;
  }

  Color color;
  bool needs_fallback = false;
  switch (css_value_id) {
    case CSSValueID::kActiveborder:
      color = GetSystemColor(MacSystemColorID::kKeyboardFocusIndicator);
      break;
    case CSSValueID::kActivecaption:
      color = GetSystemColor(MacSystemColorID::kWindowFrameText);
      break;
    case CSSValueID::kAppworkspace:
      color = GetSystemColor(MacSystemColorID::kHeader);
      break;
    case CSSValueID::kBackground:
      // Use theme independent default
      needs_fallback = true;
      break;
    case CSSValueID::kButtonface:
      color = GetSystemColor(MacSystemColorID::kControlBackground);
      break;
    case CSSValueID::kButtonhighlight:
      color = GetSystemColor(MacSystemColorID::kControlHighlight);
      break;
    case CSSValueID::kButtonshadow:
      color = GetSystemColor(MacSystemColorID::kControlShadow);
      break;
    case CSSValueID::kButtontext:
      color = GetSystemColor(MacSystemColorID::kControlText);
      break;
    case CSSValueID::kCaptiontext:
      color = GetSystemColor(MacSystemColorID::kText);
      break;
    case CSSValueID::kField:
      color = GetSystemColor(MacSystemColorID::kControlBackground);
      break;
    case CSSValueID::kFieldtext:
      color = GetSystemColor(MacSystemColorID::kText);
      break;
    case CSSValueID::kGraytext:
      color = GetSystemColor(MacSystemColorID::kDisabledControlText);
      break;
    case CSSValueID::kHighlight:
      color = GetSystemColor(MacSystemColorID::kSelectedTextBackground);
      break;
    case CSSValueID::kHighlighttext:
      color = GetSystemColor(MacSystemColorID::kSelectedText);
      break;
    case CSSValueID::kInactiveborder:
      color = GetSystemColor(MacSystemColorID::kControlBackground);
      break;
    case CSSValueID::kInactivecaption:
      color = GetSystemColor(MacSystemColorID::kControlBackground);
      break;
    case CSSValueID::kInactivecaptiontext:
      color = GetSystemColor(MacSystemColorID::kText);
      break;
    case CSSValueID::kInfobackground:
      // There is no corresponding NSColor for this so we use a hard coded
      // value.
      color = 0xFFFBFCC5;
      break;
    case CSSValueID::kInfotext:
      color = GetSystemColor(MacSystemColorID::kText);
      break;
    case CSSValueID::kMenu:
      color = GetSystemColor(MacSystemColorID::kMenuBackground);
      break;
    case CSSValueID::kMenutext:
      color = GetSystemColor(MacSystemColorID::kSelectedMenuItemText);
      break;
    case CSSValueID::kScrollbar:
      color = GetSystemColor(MacSystemColorID::kScrollBar);
      break;
    case CSSValueID::kText:
      color = GetSystemColor(MacSystemColorID::kText);
      break;
    case CSSValueID::kThreeddarkshadow:
      color = GetSystemColor(MacSystemColorID::kControlDarkShadow);
      break;
    case CSSValueID::kThreedshadow:
      color = GetSystemColor(MacSystemColorID::kShadow);
      break;
    case CSSValueID::kThreedface:
      // We use this value instead of NSColor's controlColor to avoid website
      // incompatibilities. We may want to change this to use the NSColor in
      // future.
      color = 0xFFC0C0C0;
      break;
    case CSSValueID::kThreedhighlight:
      color = GetSystemColor(MacSystemColorID::kHighlight);
      break;
    case CSSValueID::kThreedlightshadow:
      color = GetSystemColor(MacSystemColorID::kControlLightHighlight);
      break;
    case CSSValueID::kWebkitFocusRingColor:
      color = GetSystemColor(MacSystemColorID::kKeyboardFocusIndicator);
      break;
    case CSSValueID::kWindow:
    case CSSValueID::kCanvas:
      color = GetSystemColor(MacSystemColorID::kWindowBackground);
      break;
    case CSSValueID::kWindowframe:
      color = GetSystemColor(MacSystemColorID::kWindowFrame);
      break;
    case CSSValueID::kWindowtext:
    case CSSValueID::kCanvastext:
      color = GetSystemColor(MacSystemColorID::kWindowFrameText);
      break;
    default:
      needs_fallback = true;
      break;
  }

  if (needs_fallback)
    color = LayoutTheme::SystemColor(css_value_id, color_scheme);

  system_color_cache_.Set(css_value_id, color.Rgb());

  return color;
}

bool LayoutThemeMac::IsControlStyled(ControlPart part,
                                     const ComputedStyle& style) const {
  if (part == kTextFieldPart || part == kTextAreaPart)
    return style.HasAuthorBorder() || style.BoxShadow();

  if (part == kMenulistPart) {
    // FIXME: This is horrible, but there is not much else that can be done.
    // Menu lists cannot draw properly when scaled. They can't really draw
    // properly when transformed either. We can't detect the transform case
    // at style adjustment time so that will just have to stay broken.  We
    // can however detect that we're zooming. If zooming is in effect we
    // treat it like the control is styled.
    if (style.EffectiveZoom() != 1.0f)
      return true;
    if (!FontSizeMatchesToControlSize(style))
      return true;
    if (style.GetFontDescription().Family().Family() !=
        font_family_names::kSystemUi)
      return true;
    if (!style.Height().IsIntrinsicOrAuto())
      return true;
  }
  // Some other cells don't work well when scaled.
  if (style.EffectiveZoom() != 1) {
    switch (part) {
      case kButtonPart:
      case kPushButtonPart:
      case kSearchFieldPart:
      case kSquareButtonPart:
        return true;
      default:
        break;
    }
  }
  return LayoutTheme::IsControlStyled(part, style);
}

void LayoutThemeMac::AddVisualOverflow(const Node* node,
                                       const ComputedStyle& style,
                                       IntRect& rect) {
  ControlPart part = style.EffectiveAppearance();
  switch (part) {
    case kCheckboxPart:
    case kRadioPart:
    case kPushButtonPart:
    case kSquareButtonPart:
    case kButtonPart:
    case kInnerSpinButtonPart:
      return AddVisualOverflowHelper(part, ControlStatesForNode(node, style),
                                     style.EffectiveZoom(), rect);
    default:
      break;
  }

  float zoom_level = style.EffectiveZoom();

  if (part == kMenulistPart) {
    SetPopupButtonCellState(node, style, rect);
    IntSize size = PopupButtonSizes()[[PopupButton() controlSize]];
    size.SetHeight(size.Height() * zoom_level);
    size.SetWidth(rect.Width());
    rect = InflateRect(rect, size, PopupButtonMargins(), zoom_level);
  } else if (part == kSliderThumbHorizontalPart ||
             part == kSliderThumbVerticalPart) {
    rect.SetHeight(rect.Height() + kSliderThumbShadowBlur);
  }
}

void LayoutThemeMac::UpdateCheckedState(NSCell* cell, const Node* node) {
  bool old_indeterminate = [cell state] == NSMixedState;
  bool indeterminate = IsIndeterminate(node);
  bool checked = IsChecked(node);

  if (old_indeterminate != indeterminate) {
    [cell setState:indeterminate ? NSMixedState
                                 : (checked ? NSOnState : NSOffState)];
    return;
  }

  bool old_checked = [cell state] == NSOnState;
  if (checked != old_checked)
    [cell setState:checked ? NSOnState : NSOffState];
}

void LayoutThemeMac::UpdateEnabledState(NSCell* cell, const Node* node) {
  bool old_enabled = [cell isEnabled];
  bool enabled = IsEnabled(node);
  if (enabled != old_enabled)
    [cell setEnabled:enabled];
}

void LayoutThemeMac::UpdateFocusedState(NSCell* cell,
                                        const Node* node,
                                        const ComputedStyle& style) {
  bool old_focused = [cell showsFirstResponder];
  bool focused = IsFocused(node) && style.OutlineStyleIsAuto();
  if (focused != old_focused)
    [cell setShowsFirstResponder:focused];
}

void LayoutThemeMac::UpdatePressedState(NSCell* cell, const Node* node) {
  bool old_pressed = [cell isHighlighted];
  bool pressed = node && node->IsActive();
  if (pressed != old_pressed)
    [cell setHighlighted:pressed];
}

NSControlSize LayoutThemeMac::ControlSizeForFont(
    const ComputedStyle& style) const {
  int font_size = style.FontSize();
  if (font_size >= 16)
    return NSRegularControlSize;
  if (font_size >= 11)
    return NSSmallControlSize;
  return NSMiniControlSize;
}

void LayoutThemeMac::SetControlSize(NSCell* cell,
                                    const IntSize* sizes,
                                    const IntSize& min_size,
                                    float zoom_level) {
  NSControlSize size;
  if (min_size.Width() >=
          static_cast<int>(sizes[NSRegularControlSize].Width() * zoom_level) &&
      min_size.Height() >=
          static_cast<int>(sizes[NSRegularControlSize].Height() * zoom_level))
    size = NSRegularControlSize;
  else if (min_size.Width() >=
               static_cast<int>(sizes[NSSmallControlSize].Width() *
                                zoom_level) &&
           min_size.Height() >=
               static_cast<int>(sizes[NSSmallControlSize].Height() *
                                zoom_level))
    size = NSSmallControlSize;
  else
    size = NSMiniControlSize;
  // Only update if we have to, since AppKit does work even if the size is the
  // same.
  if (size != [cell controlSize])
    [cell setControlSize:size];
}

IntSize LayoutThemeMac::SizeForFont(const ComputedStyle& style,
                                    const IntSize* sizes) const {
  if (style.EffectiveZoom() != 1.0f) {
    IntSize result = sizes[ControlSizeForFont(style)];
    return IntSize(result.Width() * style.EffectiveZoom(),
                   result.Height() * style.EffectiveZoom());
  }
  return sizes[ControlSizeForFont(style)];
}

IntSize LayoutThemeMac::SizeForSystemFont(const ComputedStyle& style,
                                          const IntSize* sizes) const {
  if (style.EffectiveZoom() != 1.0f) {
    IntSize result = sizes[ControlSizeForSystemFont(style)];
    return IntSize(result.Width() * style.EffectiveZoom(),
                   result.Height() * style.EffectiveZoom());
  }
  return sizes[ControlSizeForSystemFont(style)];
}

void LayoutThemeMac::SetSizeFromFont(ComputedStyle& style,
                                     const IntSize* sizes) const {
  // FIXME: Check is flawed, since it doesn't take min-width/max-width into
  // account.
  IntSize size = SizeForFont(style, sizes);
  if (style.Width().IsIntrinsicOrAuto() && size.Width() > 0)
    style.SetWidth(Length::Fixed(size.Width()));
  if (style.Height().IsAuto() && size.Height() > 0)
    style.SetHeight(Length::Fixed(size.Height()));
}

void LayoutThemeMac::SetFontFromControlSize(ComputedStyle& style,
                                            NSControlSize control_size) const {
  FontDescription font_description;
  font_description.SetIsAbsoluteSize(true);
  font_description.SetGenericFamily(FontDescription::kSerifFamily);

  NSFont* font = [NSFont
      systemFontOfSize:[NSFont systemFontSizeForControlSize:control_size]];
  font_description.FirstFamily().SetFamily(font_family_names::kSystemUi);
  font_description.SetComputedSize([font pointSize] * style.EffectiveZoom());
  font_description.SetSpecifiedSize([font pointSize] * style.EffectiveZoom());

  // Reset line height.
  style.SetLineHeight(ComputedStyleInitialValues::InitialLineHeight());

  // TODO(esprehn): The fontSelector manual management is buggy and error prone.
  FontSelector* font_selector = style.GetFont().GetFontSelector();
  if (style.SetFontDescription(font_description))
    style.GetFont().Update(font_selector);
}

NSControlSize LayoutThemeMac::ControlSizeForSystemFont(
    const ComputedStyle& style) const {
  float font_size = style.FontSize();
  float zoom_level = style.EffectiveZoom();
  if (zoom_level != 1)
    font_size /= zoom_level;
  if (font_size >= [NSFont systemFontSizeForControlSize:NSRegularControlSize])
    return NSRegularControlSize;
  if (font_size >= [NSFont systemFontSizeForControlSize:NSSmallControlSize])
    return NSSmallControlSize;
  return NSMiniControlSize;
}

const int* LayoutThemeMac::PopupButtonMargins() const {
  static const int kMargins[3][4] = {{0, 3, 1, 3}, {0, 3, 2, 3}, {0, 1, 0, 1}};
  return kMargins[[PopupButton() controlSize]];
}

const IntSize* LayoutThemeMac::PopupButtonSizes() const {
  static const IntSize kSizes[3] = {IntSize(0, 21), IntSize(0, 18),
                                    IntSize(0, 15)};
  return kSizes;
}

const int* LayoutThemeMac::PopupButtonPadding(NSControlSize size) const {
  static const int kPadding[3][4] = {
      {2, 26, 3, 8}, {2, 23, 3, 8}, {2, 22, 3, 10}};
  return kPadding[size];
}

const int* LayoutThemeMac::ProgressBarHeights() const {
  static const int kSizes[3] = {20, 12, 12};
  return kSizes;
}

constexpr base::TimeDelta LayoutThemeMac::kProgressAnimationFrameRate;

base::TimeDelta LayoutThemeMac::AnimationRepeatIntervalForProgressBar() const {
  return kProgressAnimationFrameRate;
}

base::TimeDelta LayoutThemeMac::AnimationDurationForProgressBar() const {
  return kProgressAnimationNumFrames * kProgressAnimationFrameRate;
}

static const IntSize* MenuListButtonSizes() {
  static const IntSize kSizes[3] = {IntSize(0, 21), IntSize(0, 18),
                                    IntSize(0, 15)};
  return kSizes;
}

void LayoutThemeMac::AdjustMenuListStyle(ComputedStyle& style,
                                         Element* e) const {
  NSControlSize control_size = ControlSizeForFont(style);

  style.ResetBorder();
  style.ResetPadding();

  // Height is locked to auto.
  style.SetHeight(Length::Auto());

  // White-space is locked to pre.
  style.SetWhiteSpace(EWhiteSpace::kPre);

  // Set the foreground color to black or gray when we have the aqua look.
  // Cast to RGB32 is to work around a compiler bug.
  style.SetColor(e && !e->IsDisabledFormControl()
                     ? static_cast<RGBA32>(Color::kBlack)
                     : Color::kDarkGray);

  // Set the button's vertical size.
  SetSizeFromFont(style, MenuListButtonSizes());

  // Our font is locked to the appropriate system font size for the
  // control. To clarify, we first use the CSS-specified font to figure out a
  // reasonable control size, but once that control size is determined, we
  // throw that font away and use the appropriate system font for the control
  // size instead.
  SetFontFromControlSize(style, control_size);
}

static const int kBaseBorderRadius = 5;
static const int kStyledPopupPaddingStart = 8;
static const int kStyledPopupPaddingTop = 1;
static const int kStyledPopupPaddingBottom = 2;

// These functions are called with MenuListPart or MenulistButtonPart appearance
// by LayoutMenuList.
int LayoutThemeMac::PopupInternalPaddingStart(
    const ComputedStyle& style) const {
  if (style.EffectiveAppearance() == kMenulistPart)
    return PopupButtonPadding(ControlSizeForFont(style))[kLeftMargin] *
           style.EffectiveZoom();
  if (style.EffectiveAppearance() == kMenulistButtonPart)
    return kStyledPopupPaddingStart * style.EffectiveZoom();
  return 0;
}

int LayoutThemeMac::PopupInternalPaddingEnd(LocalFrame*,
                                            const ComputedStyle& style) const {
  if (style.EffectiveAppearance() == kMenulistPart)
    return PopupButtonPadding(ControlSizeForFont(style))[kRightMargin] *
           style.EffectiveZoom();
  if (style.EffectiveAppearance() != kMenulistButtonPart)
    return 0;
  float font_scale = style.FontSize() / kBaseFontSize;
  float arrow_width = kMenuListBaseArrowWidth * font_scale;
  return static_cast<int>(ceilf(
      arrow_width + (kMenuListArrowPaddingStart + kMenuListArrowPaddingEnd) *
                        style.EffectiveZoom()));
}

int LayoutThemeMac::PopupInternalPaddingTop(const ComputedStyle& style) const {
  if (style.EffectiveAppearance() == kMenulistPart)
    return PopupButtonPadding(ControlSizeForFont(style))[kTopMargin] *
           style.EffectiveZoom();
  if (style.EffectiveAppearance() == kMenulistButtonPart)
    return kStyledPopupPaddingTop * style.EffectiveZoom();
  return 0;
}

int LayoutThemeMac::PopupInternalPaddingBottom(
    const ComputedStyle& style) const {
  if (style.EffectiveAppearance() == kMenulistPart)
    return PopupButtonPadding(ControlSizeForFont(style))[kBottomMargin] *
           style.EffectiveZoom();
  if (style.EffectiveAppearance() == kMenulistButtonPart)
    return kStyledPopupPaddingBottom * style.EffectiveZoom();
  return 0;
}

void LayoutThemeMac::AdjustMenuListButtonStyle(ComputedStyle& style,
                                               Element*) const {
  float font_scale = style.FontSize() / kBaseFontSize;

  style.ResetPadding();
  style.SetBorderRadius(
      IntSize(int(kBaseBorderRadius + font_scale - 1),
              int(kBaseBorderRadius + font_scale - 1)));  // FIXME: Round up?

  const int kMinHeight = 15;
  style.SetMinHeight(Length::Fixed(kMinHeight));

  style.SetLineHeight(ComputedStyleInitialValues::InitialLineHeight());
}

void LayoutThemeMac::SetPopupButtonCellState(const Node* node,
                                             const ComputedStyle& style,
                                             const IntRect& rect) {
  NSPopUpButtonCell* popup_button = this->PopupButton();

  // Set the control size based off the rectangle we're painting into.
  SetControlSize(popup_button, PopupButtonSizes(), rect.Size(),
                 style.EffectiveZoom());

  // Update the various states we respond to.
  UpdateActiveState(popup_button, node);
  UpdateCheckedState(popup_button, node);
  UpdateEnabledState(popup_button, node);
  UpdatePressedState(popup_button, node);

  popup_button.userInterfaceLayoutDirection =
      style.Direction() == TextDirection::kLtr
          ? NSUserInterfaceLayoutDirectionLeftToRight
          : NSUserInterfaceLayoutDirectionRightToLeft;
}

const IntSize* LayoutThemeMac::MenuListSizes() const {
  static const IntSize kSizes[3] = {IntSize(9, 0), IntSize(5, 0),
                                    IntSize(0, 0)};
  return kSizes;
}

int LayoutThemeMac::MinimumMenuListSize(const ComputedStyle& style) const {
  return SizeForSystemFont(style, MenuListSizes()).Width();
}

void LayoutThemeMac::SetSearchCellState(const Node* node,
                                        const ComputedStyle& style,
                                        const IntRect&) {
  NSSearchFieldCell* search = this->Search();

  // Update the various states we respond to.
  UpdateActiveState(search, node);
  UpdateEnabledState(search, node);
  UpdateFocusedState(search, node, style);
}

const IntSize* LayoutThemeMac::SearchFieldSizes() const {
  static const IntSize kSizes[3] = {IntSize(0, 22), IntSize(0, 19),
                                    IntSize(0, 15)};
  return kSizes;
}

static const int* SearchFieldHorizontalPaddings() {
  static const int kSizes[3] = {3, 2, 1};
  return kSizes;
}

void LayoutThemeMac::SetSearchFieldSize(ComputedStyle& style) const {
  // If the width and height are both specified, then we have nothing to do.
  if (!style.Width().IsIntrinsicOrAuto() && !style.Height().IsAuto())
    return;

  // Use the font size to determine the intrinsic width of the control.
  SetSizeFromFont(style, SearchFieldSizes());
}

const uint8_t kSearchFieldBorderWidth = 2;
void LayoutThemeMac::AdjustSearchFieldStyle(ComputedStyle& style) const {
  // Override border.
  style.ResetBorder();
  const float border_width = kSearchFieldBorderWidth * style.EffectiveZoom();
  style.SetBorderLeftWidth(border_width);
  style.SetBorderLeftStyle(EBorderStyle::kInset);
  style.SetBorderRightWidth(border_width);
  style.SetBorderRightStyle(EBorderStyle::kInset);
  style.SetBorderBottomWidth(border_width);
  style.SetBorderBottomStyle(EBorderStyle::kInset);
  style.SetBorderTopWidth(border_width);
  style.SetBorderTopStyle(EBorderStyle::kInset);

  // Override height.
  style.SetHeight(Length::Auto());
  SetSearchFieldSize(style);

  NSControlSize control_size = ControlSizeForFont(style);

  // Override padding size to match AppKit text positioning.
  const int vertical_padding = 1 * style.EffectiveZoom();
  const int horizontal_padding =
      SearchFieldHorizontalPaddings()[control_size] * style.EffectiveZoom();
  style.SetPaddingLeft(Length::Fixed(horizontal_padding));
  style.SetPaddingRight(Length::Fixed(horizontal_padding));
  style.SetPaddingTop(Length::Fixed(vertical_padding));
  style.SetPaddingBottom(Length::Fixed(vertical_padding));

  SetFontFromControlSize(style, control_size);

  style.SetBoxShadow(nullptr);
}

const IntSize* LayoutThemeMac::CancelButtonSizes() const {
  static const IntSize kSizes[3] = {IntSize(14, 14), IntSize(11, 11),
                                    IntSize(9, 9)};
  return kSizes;
}

void LayoutThemeMac::AdjustSearchFieldCancelButtonStyle(
    ComputedStyle& style) const {
  IntSize size = SizeForSystemFont(style, CancelButtonSizes());
  style.SetWidth(Length::Fixed(size.Width()));
  style.SetHeight(Length::Fixed(size.Height()));
  style.SetBoxShadow(nullptr);
}

IntSize LayoutThemeMac::SliderTickSize() const {
  return IntSize(1, 3);
}

int LayoutThemeMac::SliderTickOffsetFromTrackCenter() const {
  return -9;
}

void LayoutThemeMac::AdjustProgressBarBounds(ComputedStyle& style) const {
  float zoom_level = style.EffectiveZoom();
  NSControlSize control_size = ControlSizeForFont(style);
  int height = ProgressBarHeights()[control_size] * zoom_level;

  // Now inflate it to account for the shadow.
  style.SetMinHeight(Length::Fixed(height + zoom_level));
}

void LayoutThemeMac::AdjustSliderThumbSize(ComputedStyle& style) const {
  float zoom_level = style.EffectiveZoom();
  if (style.EffectiveAppearance() == kSliderThumbHorizontalPart ||
      style.EffectiveAppearance() == kSliderThumbVerticalPart) {
    style.SetWidth(
        Length::Fixed(static_cast<int>(kSliderThumbWidth * zoom_level)));
    style.SetHeight(
        Length::Fixed(static_cast<int>(kSliderThumbHeight * zoom_level)));
  }
}

NSPopUpButtonCell* LayoutThemeMac::PopupButton() const {
  if (!popup_button_) {
    popup_button_.reset([[NSPopUpButtonCell alloc] initTextCell:@""
                                                      pullsDown:NO]);
    [popup_button_ setUsesItemFromMenu:NO];
    [popup_button_ setFocusRingType:NSFocusRingTypeExterior];
  }

  return popup_button_;
}

NSSearchFieldCell* LayoutThemeMac::Search() const {
  if (!search_) {
    search_.reset([[NSSearchFieldCell alloc] initTextCell:@""]);
    [search_ setBezelStyle:NSTextFieldRoundedBezel];
    [search_ setBezeled:YES];
    [search_ setEditable:YES];
    [search_ setFocusRingType:NSFocusRingTypeExterior];

    // Suppress NSSearchFieldCell's default placeholder text. Prior to OS10.11,
    // this is achieved by calling |setCenteredLook| with NO. In OS10.11 and
    // later, instead call |setPlaceholderString| with an empty string.
    // See https://crbug.com/752362.
    if (base::mac::IsAtMostOS10_10()) {
      SEL sel = @selector(setCenteredLook:);
      if ([search_ respondsToSelector:sel]) {
        BOOL bool_value = NO;
        NSMethodSignature* signature =
            [NSSearchFieldCell instanceMethodSignatureForSelector:sel];
        NSInvocation* invocation =
            [NSInvocation invocationWithMethodSignature:signature];
        [invocation setTarget:search_];
        [invocation setSelector:sel];
        [invocation setArgument:&bool_value atIndex:2];
        [invocation invoke];
      }
    } else {
      [search_ setPlaceholderString:@""];
    }
  }

  return search_;
}

NSTextFieldCell* LayoutThemeMac::TextField() const {
  if (!text_field_) {
    text_field_.reset([[BlinkTextFieldCell alloc] initTextCell:@""]);
    [text_field_ setBezeled:YES];
    [text_field_ setEditable:YES];
    [text_field_ setFocusRingType:NSFocusRingTypeExterior];
    [text_field_ setDrawsBackground:YES];
    [text_field_ setBackgroundColor:[NSColor whiteColor]];
  }

  return text_field_;
}

String LayoutThemeMac::FileListNameForWidth(Locale& locale,
                                            const FileList* file_list,
                                            const Font& font,
                                            int width) const {
  if (width <= 0)
    return String();

  String str_to_truncate;
  if (file_list->IsEmpty()) {
    str_to_truncate = locale.QueryString(IDS_FORM_FILE_NO_FILE_LABEL);
  } else if (file_list->length() == 1) {
    File* file = file_list->item(0);
    if (file->GetUserVisibility() == File::kIsUserVisible)
      str_to_truncate = [[NSFileManager defaultManager]
          displayNameAtPath:(file_list->item(0)->GetPath())];
    else
      str_to_truncate = file->name();
  } else {
    return StringTruncator::RightTruncate(
        locale.QueryString(IDS_FORM_FILE_MULTIPLE_UPLOAD,
                           locale.ConvertToLocalizedNumber(
                               String::Number(file_list->length()))),
        width, font);
  }

  return StringTruncator::CenterTruncate(str_to_truncate, width, font);
}

NSView* FlippedView() {
  static NSView* view = [[BlinkFlippedView alloc] init];
  return view;
}

LayoutTheme& LayoutTheme::NativeTheme() {
  if (RuntimeEnabledFeatures::FormControlsRefreshEnabled()) {
    DEFINE_STATIC_REF(LayoutTheme, layout_theme,
                      (LayoutThemeMacRefresh::Create()));
    return *layout_theme;
  } else {
    DEFINE_STATIC_REF(LayoutTheme, layout_theme, (LayoutThemeMac::Create()));
    return *layout_theme;
  }
}

scoped_refptr<LayoutTheme> LayoutThemeMac::Create() {
  return base::AdoptRef(new LayoutThemeMac);
}

bool LayoutThemeMac::UsesTestModeFocusRingColor() const {
  return WebTestSupport::IsRunningWebTest();
}

NSView* LayoutThemeMac::DocumentView() const {
  return FlippedView();
}

// Updates the control tint (a.k.a. active state) of |cell| (from |o|).  In the
// Chromium port, the layoutObject runs as a background process and controls'
// NSCell(s) lack a parent NSView. Therefore controls don't have their tint
// color updated correctly when the application is activated/deactivated.
// FocusController's setActive() is called when the application is
// activated/deactivated, which causes a paint invalidation at which time this
// code is called.
// This function should be called before drawing any NSCell-derived controls,
// unless you're sure it isn't needed.
void LayoutThemeMac::UpdateActiveState(NSCell* cell, const Node* node) {
  NSControlTint old_tint = [cell controlTint];
  NSControlTint tint = IsActive(node)
                           ? [NSColor currentControlTint]
                           : static_cast<NSControlTint>(NSClearControlTint);

  if (tint != old_tint)
    [cell setControlTint:tint];
}

String LayoutThemeMac::ExtraFullscreenStyleSheet() {
  // FIXME: Chromium may wish to style its default media controls differently in
  // fullscreen.
  return String();
}

String LayoutThemeMac::ExtraDefaultStyleSheet() {
  return LayoutTheme::ExtraDefaultStyleSheet() +
         UncompressResourceAsASCIIString(
             IDR_UASTYLE_THEME_INPUT_MULTIPLE_FIELDS_CSS) +
         UncompressResourceAsASCIIString(IDR_UASTYLE_THEME_MAC_CSS);
}

bool LayoutThemeMac::ThemeDrawsFocusRing(const ComputedStyle& style) const {
  if (ShouldUseFallbackTheme(style))
    return false;
  switch (style.EffectiveAppearance()) {
    case kCheckboxPart:
    case kRadioPart:
    case kPushButtonPart:
    case kSquareButtonPart:
    case kButtonPart:
    case kMenulistPart:
    case kSliderThumbHorizontalPart:
    case kSliderThumbVerticalPart:
      return true;

    // Actually, they don't support native focus rings, but this function
    // returns true for them in order to prevent Blink from drawing focus rings.
    // SliderThumb*Part have focus rings, and we don't need to draw two focus
    // rings for single slider.
    case kSliderHorizontalPart:
    case kSliderVerticalPart:
      return true;

    default:
      return false;
  }
}

bool LayoutThemeMac::ShouldUseFallbackTheme(const ComputedStyle& style) const {
  ControlPart part = style.EffectiveAppearance();
  if (part == kCheckboxPart || part == kRadioPart)
    return style.EffectiveZoom() != 1;
  return false;
}

// We don't use controlSizeForFont() for steppers because the stepper height
// should be equal to or less than the corresponding text field height,
static NSControlSize StepperControlSizeForFont(
    const FontDescription& font_description) {
  int font_size = font_description.ComputedPixelSize();
  if (font_size >= 27)
    return NSRegularControlSize;
  if (font_size >= 22)
    return NSSmallControlSize;
  return NSMiniControlSize;
}

NSControlSize LayoutThemeMac::ControlSizeFromPixelSize(
    const IntSize* sizes,
    const IntSize& min_zoomed_size,
    float zoom_factor) {
  if (min_zoomed_size.Width() >=
          static_cast<int>(sizes[NSRegularControlSize].Width() * zoom_factor) &&
      min_zoomed_size.Height() >=
          static_cast<int>(sizes[NSRegularControlSize].Height() * zoom_factor))
    return NSRegularControlSize;
  if (min_zoomed_size.Width() >=
          static_cast<int>(sizes[NSSmallControlSize].Width() * zoom_factor) &&
      min_zoomed_size.Height() >=
          static_cast<int>(sizes[NSSmallControlSize].Height() * zoom_factor))
    return NSSmallControlSize;
  return NSMiniControlSize;
}

static void SetControlSizeThemeMac(NSCell* cell,
                                   const IntSize* sizes,
                                   const IntSize& min_zoomed_size,
                                   float zoom_factor) {
  ControlSize size = LayoutThemeMac::ControlSizeFromPixelSize(
      sizes, min_zoomed_size, zoom_factor);
  // Only update if we have to, since AppKit does work even if the size is the
  // same.
  if (size != [cell controlSize])
    [cell setControlSize:(NSControlSize)size];
}

static void UpdateStates(NSCell* cell, ControlStates states) {
  // Hover state is not supported by Aqua.

  // Pressed state
  bool old_pressed = [cell isHighlighted];
  bool pressed = states & kPressedControlState;
  if (pressed != old_pressed)
    [cell setHighlighted:pressed];

  // Enabled state
  bool old_enabled = [cell isEnabled];
  bool enabled = states & kEnabledControlState;
  if (enabled != old_enabled)
    [cell setEnabled:enabled];

  // Checked and Indeterminate
  bool old_indeterminate = [cell state] == NSMixedState;
  bool indeterminate = (states & kIndeterminateControlState);
  bool checked = states & kCheckedControlState;
  bool old_checked = [cell state] == NSOnState;
  if (old_indeterminate != indeterminate || checked != old_checked)
    [cell setState:indeterminate ? NSMixedState
                                 : (checked ? NSOnState : NSOffState)];

  // Window inactive state does not need to be checked explicitly, since we
  // paint parented to a view in a window whose key state can be detected.
}

// Return a fake NSView whose sole purpose is to tell AppKit that it's flipped.
NSView* LayoutThemeMac::EnsuredView(const IntSize& size) {
  // Use a fake flipped view.
  static NSView* flipped_view = [[BlinkFlippedControl alloc] init];
  [flipped_view setFrameSize:NSSizeFromCGSize(CGSize(size))];

  return flipped_view;
}

LayoutUnit LayoutThemeMac::BaselinePositionAdjustment(
    const ComputedStyle& style) const {
  ControlPart part = style.EffectiveAppearance();
  if (part == kCheckboxPart || part == kRadioPart)
    return LayoutUnit(style.EffectiveZoom() * -2);
  return LayoutTheme::BaselinePositionAdjustment(style);
}

FontDescription LayoutThemeMac::ControlFont(
    ControlPart part,
    const FontDescription& font_description,
    float zoom_factor) const {
  using ::blink::ControlSizeForFont;
  switch (part) {
    case kPushButtonPart: {
      FontDescription result;
      result.SetIsAbsoluteSize(true);
      result.SetGenericFamily(FontDescription::kSerifFamily);

      NSFont* ns_font = [NSFont
          systemFontOfSize:[NSFont systemFontSizeForControlSize:
                                       ControlSizeForFont(font_description)]];
      result.FirstFamily().SetFamily(font_family_names::kSystemUi);
      result.SetComputedSize([ns_font pointSize] * zoom_factor);
      result.SetSpecifiedSize([ns_font pointSize] * zoom_factor);
      return result;
    }
    default:
      return LayoutTheme::ControlFont(part, font_description, zoom_factor);
  }
}

LengthSize LayoutThemeMac::GetControlSize(
    ControlPart part,
    const FontDescription& font_description,
    const LengthSize& zoomed_size,
    float zoom_factor) const {
  switch (part) {
    case kCheckboxPart:
      return CheckboxSize(font_description, zoomed_size, zoom_factor);
    case kRadioPart:
      return RadioSize(font_description, zoomed_size, zoom_factor);
    case kPushButtonPart:
      // Height is reset to auto so that specified heights can be ignored.
      return SizeFromFont(font_description,
                          LengthSize(zoomed_size.Width(), Length()),
                          zoom_factor, ButtonSizes());
    case kInnerSpinButtonPart:
      if (!zoomed_size.Width().IsIntrinsicOrAuto() &&
          !zoomed_size.Height().IsIntrinsicOrAuto())
        return zoomed_size;
      return SizeFromNSControlSize(StepperControlSizeForFont(font_description),
                                   zoomed_size, zoom_factor, StepperSizes());
    default:
      return zoomed_size;
  }
}

LengthSize LayoutThemeMac::MinimumControlSize(
    ControlPart part,
    const FontDescription& font_description,
    float zoom_factor,
    const ComputedStyle& style) const {
  switch (part) {
    case kSquareButtonPart:
    case kButtonPart:
      return LengthSize(style.MinWidth().Zoom(zoom_factor),
                        Length::Fixed(static_cast<int>(15 * zoom_factor)));
    case kInnerSpinButtonPart: {
      IntSize base = StepperSizes()[NSMiniControlSize];
      return LengthSize(
          Length::Fixed(static_cast<int>(base.Width() * zoom_factor)),
          Length::Fixed(static_cast<int>(base.Height() * zoom_factor)));
    }
    default:
      return LayoutTheme::MinimumControlSize(part, font_description,
                                             zoom_factor, style);
  }
}

LengthBox LayoutThemeMac::ControlPadding(
    ControlPart part,
    const FontDescription& font_description,
    const Length& zoomed_box_top,
    const Length& zoomed_box_right,
    const Length& zoomed_box_bottom,
    const Length& zoomed_box_left,
    float zoom_factor) const {
  switch (part) {
    case kPushButtonPart: {
      // Just use 8px.  AppKit wants to use 11px for mini buttons, but that
      // padding is just too large for real-world Web sites (creating a huge
      // necessary minimum width for buttons whose space is by definition
      // constrained, since we select mini only for small cramped environments.
      // This also guarantees the HTML <button> will match our rendering by
      // default, since we're using a consistent padding.
      const int padding = 8 * zoom_factor;
      return LengthBox(2, padding, 3, padding);
    }
    default:
      return LayoutTheme::ControlPadding(part, font_description, zoomed_box_top,
                                         zoomed_box_right, zoomed_box_bottom,
                                         zoomed_box_left, zoom_factor);
  }
}

LengthBox LayoutThemeMac::ControlBorder(ControlPart part,
                                        const FontDescription& font_description,
                                        const LengthBox& zoomed_box,
                                        float zoom_factor) const {
  switch (part) {
    case kSquareButtonPart:
      return LengthBox(0, zoomed_box.Right().Value(), 0,
                       zoomed_box.Left().Value());
    default:
      return LayoutTheme::ControlBorder(part, font_description, zoomed_box,
                                        zoom_factor);
  }
}

void LayoutThemeMac::AddVisualOverflowHelper(ControlPart part,
                                             ControlStates states,
                                             float zoom_factor,
                                             IntRect& zoomed_rect) const {
  BEGIN_BLOCK_OBJC_EXCEPTIONS
  switch (part) {
    case kCheckboxPart: {
      // We inflate the rect as needed to account for padding included in the
      // cell to accommodate the checkbox shadow" and the check.  We don't
      // consider this part of the bounds of the control in WebKit.
      NSCell* cell = Checkbox(states, zoomed_rect, zoom_factor);
      NSControlSize control_size = [cell controlSize];
      IntSize zoomed_size = CheckboxSizes()[control_size];
      zoomed_size.SetHeight(zoomed_size.Height() * zoom_factor);
      zoomed_size.SetWidth(zoomed_size.Width() * zoom_factor);
      zoomed_rect = InflateRect(zoomed_rect, zoomed_size,
                                CheckboxMargins(control_size), zoom_factor);
      break;
    }
    case kRadioPart: {
      // We inflate the rect as needed to account for padding included in the
      // cell to accommodate the radio button shadow".  We don't consider this
      // part of the bounds of the control in WebKit.
      NSCell* cell = Radio(states, zoomed_rect, zoom_factor);
      NSControlSize control_size = [cell controlSize];
      IntSize zoomed_size = RadioSizes()[control_size];
      zoomed_size.SetHeight(zoomed_size.Height() * zoom_factor);
      zoomed_size.SetWidth(zoomed_size.Width() * zoom_factor);
      zoomed_rect = InflateRect(zoomed_rect, zoomed_size,
                                RadioMargins(control_size), zoom_factor);
      break;
    }
    case kPushButtonPart:
    case kButtonPart: {
      NSButtonCell* cell = Button(part, states, zoomed_rect, zoom_factor);
      NSControlSize control_size = [cell controlSize];

      // We inflate the rect as needed to account for the Aqua button's shadow.
      if ([cell bezelStyle] == NSRoundedBezelStyle) {
        IntSize zoomed_size = ButtonSizes()[control_size];
        zoomed_size.SetHeight(zoomed_size.Height() * zoom_factor);
        // Buttons don't ever constrain width, so the zoomed width can just be
        // honored.
        zoomed_size.SetWidth(zoomed_rect.Width());
        zoomed_rect = InflateRect(zoomed_rect, zoomed_size,
                                  ButtonMargins(control_size), zoom_factor);
      }
      break;
    }
    case kInnerSpinButtonPart: {
      static const int kStepperMargin[4] = {0, 0, 0, 0};
      ControlSize control_size = ControlSizeFromPixelSize(
          StepperSizes(), zoomed_rect.Size(), zoom_factor);
      IntSize zoomed_size = StepperSizes()[control_size];
      zoomed_size.SetHeight(zoomed_size.Height() * zoom_factor);
      zoomed_size.SetWidth(zoomed_size.Width() * zoom_factor);
      zoomed_rect =
          InflateRect(zoomed_rect, zoomed_size, kStepperMargin, zoom_factor);
      break;
    }
    default:
      break;
  }
  END_BLOCK_OBJC_EXCEPTIONS
}

void LayoutThemeMac::AdjustControlPartStyle(ComputedStyle& style) {
  ControlPart part = style.EffectiveAppearance();
  switch (part) {
    case kCheckboxPart:
    case kInnerSpinButtonPart:
    case kRadioPart:
    case kPushButtonPart:
    case kSquareButtonPart:
    case kButtonPart: {
      // Border
      LengthBox border_box(style.BorderTopWidth(), style.BorderRightWidth(),
                           style.BorderBottomWidth(), style.BorderLeftWidth());
      border_box = ControlBorder(part, style.GetFont().GetFontDescription(),
                                 border_box, style.EffectiveZoom());
      if (border_box.Top().Value() !=
          static_cast<int>(style.BorderTopWidth())) {
        if (border_box.Top().Value())
          style.SetBorderTopWidth(border_box.Top().Value());
        else
          style.ResetBorderTop();
      }
      if (border_box.Right().Value() !=
          static_cast<int>(style.BorderRightWidth())) {
        if (border_box.Right().Value())
          style.SetBorderRightWidth(border_box.Right().Value());
        else
          style.ResetBorderRight();
      }
      if (border_box.Bottom().Value() !=
          static_cast<int>(style.BorderBottomWidth())) {
        style.SetBorderBottomWidth(border_box.Bottom().Value());
        if (border_box.Bottom().Value())
          style.SetBorderBottomWidth(border_box.Bottom().Value());
        else
          style.ResetBorderBottom();
      }
      if (border_box.Left().Value() !=
          static_cast<int>(style.BorderLeftWidth())) {
        style.SetBorderLeftWidth(border_box.Left().Value());
        if (border_box.Left().Value())
          style.SetBorderLeftWidth(border_box.Left().Value());
        else
          style.ResetBorderLeft();
      }

      // Padding
      LengthBox padding_box = ControlPadding(
          part, style.GetFont().GetFontDescription(), style.PaddingTop(),
          style.PaddingRight(), style.PaddingBottom(), style.PaddingLeft(),
          style.EffectiveZoom());
      if (!style.PaddingEqual(padding_box))
        style.SetPadding(padding_box);

      // Whitespace
      if (ControlRequiresPreWhiteSpace(part))
        style.SetWhiteSpace(EWhiteSpace::kPre);

      // Width / Height
      // The width and height here are affected by the zoom.
      LengthSize control_size = GetControlSize(
          part, style.GetFont().GetFontDescription(),
          LengthSize(style.Width(), style.Height()), style.EffectiveZoom());

      LengthSize min_control_size =
          MinimumControlSize(part, style.GetFont().GetFontDescription(),
                             style.EffectiveZoom(), style);

      // Only potentially set min-size to |control_size| for these parts.
      if (part == kCheckboxPart || part == kRadioPart)
        SetMinimumSize(style, &control_size, &min_control_size);
      else
        SetMinimumSize(style, nullptr, &min_control_size);

      if (control_size.Width() != style.Width())
        style.SetWidth(control_size.Width());
      if (control_size.Height() != style.Height())
        style.SetHeight(control_size.Height());

      // Font
      FontDescription control_font = ControlFont(
          part, style.GetFont().GetFontDescription(), style.EffectiveZoom());
      if (control_font != style.GetFont().GetFontDescription()) {
        // Reset our line-height
        style.SetLineHeight(ComputedStyleInitialValues::InitialLineHeight());

        // Now update our font.
        if (style.SetFontDescription(control_font))
          style.GetFont().Update(nullptr);
      }
      break;
    }
    case kProgressBarPart:
      AdjustProgressBarBounds(style);
      break;
    default:
      break;
  }
}

// static
IntRect LayoutThemeMac::InflateRect(const IntRect& zoomed_rect,
                                    const IntSize& zoomed_size,
                                    const int* margins,
                                    float zoom_factor) {
  // Only do the inflation if the available width/height are too small.
  // Otherwise try to fit the glow/check space into the available box's
  // width/height.
  int width_delta = zoomed_rect.Width() -
                    (zoomed_size.Width() + margins[kLeftMargin] * zoom_factor +
                     margins[kRightMargin] * zoom_factor);
  int height_delta = zoomed_rect.Height() -
                     (zoomed_size.Height() + margins[kTopMargin] * zoom_factor +
                      margins[kBottomMargin] * zoom_factor);
  IntRect result(zoomed_rect);
  if (width_delta < 0) {
    result.SetX(result.X() - margins[kLeftMargin] * zoom_factor);
    result.SetWidth(result.Width() - width_delta);
  }
  if (height_delta < 0) {
    result.SetY(result.Y() - margins[kTopMargin] * zoom_factor);
    result.SetHeight(result.Height() - height_delta);
  }
  return result;
}

// static
IntRect LayoutThemeMac::InflateRectForFocusRing(const IntRect& rect) {
  // Just put a margin of 16 units around the rect. The UI elements that use
  // this don't appropriately scale their focus rings appropriately (e.g, paint
  // pickers), or switch to non-native widgets when scaled (e.g, check boxes
  // and radio buttons).
  const int kMargin = 16;
  IntRect result;
  result.SetX(rect.X() - kMargin);
  result.SetY(rect.Y() - kMargin);
  result.SetWidth(rect.Width() + 2 * kMargin);
  result.SetHeight(rect.Height() + 2 * kMargin);
  return result;
}

// Checkboxes

const IntSize* LayoutThemeMac::CheckboxSizes() {
  static const IntSize kSizes[3] = {IntSize(14, 14), IntSize(12, 12),
                                    IntSize(10, 10)};
  return kSizes;
}

const int* LayoutThemeMac::CheckboxMargins(NSControlSize control_size) {
  static const int kMargins[3][4] = {
      {3, 4, 4, 2},
      {4, 3, 3, 3},
      {4, 3, 3, 3},
  };
  return kMargins[control_size];
}

LengthSize LayoutThemeMac::CheckboxSize(const FontDescription& font_description,
                                        const LengthSize& zoomed_size,
                                        float zoom_factor) {
  // If the width and height are both specified, then we have nothing to do.
  if (!zoomed_size.Width().IsIntrinsicOrAuto() &&
      !zoomed_size.Height().IsIntrinsicOrAuto())
    return zoomed_size;

  // Use the font size to determine the intrinsic width of the control.
  return SizeFromFont(font_description, zoomed_size, zoom_factor,
                      CheckboxSizes());
}

NSButtonCell* LayoutThemeMac::Checkbox(ControlStates states,
                                       const IntRect& zoomed_rect,
                                       float zoom_factor) {
  static NSButtonCell* checkbox_cell;
  if (!checkbox_cell) {
    checkbox_cell = [[NSButtonCell alloc] init];
    [checkbox_cell setButtonType:NSSwitchButton];
    [checkbox_cell setTitle:nil];
    [checkbox_cell setAllowsMixedState:YES];
    [checkbox_cell setFocusRingType:NSFocusRingTypeExterior];
  }

  // Set the control size based off the rectangle we're painting into.
  SetControlSizeThemeMac(checkbox_cell, CheckboxSizes(), zoomed_rect.Size(),
                         zoom_factor);

  // Update the various states we respond to.
  UpdateStates(checkbox_cell, states);

  return checkbox_cell;
}

const IntSize* LayoutThemeMac::RadioSizes() {
  static const IntSize kSizes[3] = {IntSize(14, 15), IntSize(12, 13),
                                    IntSize(10, 10)};
  return kSizes;
}

const int* LayoutThemeMac::RadioMargins(NSControlSize control_size) {
  static const int kMargins[3][4] = {
      {2, 2, 4, 2},
      {3, 2, 3, 2},
      {1, 0, 2, 0},
  };
  return kMargins[control_size];
}

LengthSize LayoutThemeMac::RadioSize(const FontDescription& font_description,
                                     const LengthSize& zoomed_size,
                                     float zoom_factor) {
  // If the width and height are both specified, then we have nothing to do.
  if (!zoomed_size.Width().IsIntrinsicOrAuto() &&
      !zoomed_size.Height().IsIntrinsicOrAuto())
    return zoomed_size;

  // Use the font size to determine the intrinsic width of the control.
  return SizeFromFont(font_description, zoomed_size, zoom_factor, RadioSizes());
}

NSButtonCell* LayoutThemeMac::Radio(ControlStates states,
                                    const IntRect& zoomed_rect,
                                    float zoom_factor) {
  static NSButtonCell* radio_cell;
  if (!radio_cell) {
    radio_cell = [[NSButtonCell alloc] init];
    [radio_cell setButtonType:NSRadioButton];
    [radio_cell setTitle:nil];
    [radio_cell setFocusRingType:NSFocusRingTypeExterior];
  }

  // Set the control size based off the rectangle we're painting into.
  SetControlSizeThemeMac(radio_cell, RadioSizes(), zoomed_rect.Size(),
                         zoom_factor);

  // Update the various states we respond to.
  // Cocoa draws NSMixedState NSRadioButton as NSOnState so we don't want that.
  states &= ~kIndeterminateControlState;
  UpdateStates(radio_cell, states);

  return radio_cell;
}

// Buttons really only constrain height. They respect width.
const IntSize* LayoutThemeMac::ButtonSizes() {
  static const IntSize kSizes[3] = {IntSize(0, 21), IntSize(0, 18),
                                    IntSize(0, 15)};
  return kSizes;
}

const int* LayoutThemeMac::ButtonMargins(NSControlSize control_size) {
  static const int kMargins[3][4] = {
      {4, 6, 7, 6},
      {4, 5, 6, 5},
      {0, 1, 1, 1},
  };
  return kMargins[control_size];
}

static void SetUpButtonCell(NSButtonCell* cell,
                            ControlPart part,
                            ControlStates states,
                            const IntRect& zoomed_rect,
                            float zoom_factor) {
  // Set the control size based off the rectangle we're painting into.
  const IntSize* sizes = LayoutThemeMac::ButtonSizes();
  if (part == kSquareButtonPart ||
      zoomed_rect.Height() >
          LayoutThemeMac::ButtonSizes()[NSRegularControlSize].Height() *
              zoom_factor) {
    // Use the square button
    if ([cell bezelStyle] != NSShadowlessSquareBezelStyle)
      [cell setBezelStyle:NSShadowlessSquareBezelStyle];
  } else if ([cell bezelStyle] != NSRoundedBezelStyle)
    [cell setBezelStyle:NSRoundedBezelStyle];

  SetControlSizeThemeMac(cell, sizes, zoomed_rect.Size(), zoom_factor);

  // Update the various states we respond to.
  UpdateStates(cell, states);
}

NSButtonCell* LayoutThemeMac::Button(ControlPart part,
                                     ControlStates states,
                                     const IntRect& zoomed_rect,
                                     float zoom_factor) {
  static NSButtonCell* cell = nil;
  if (!cell) {
    cell = [[NSButtonCell alloc] init];
    [cell setTitle:nil];
    [cell setButtonType:NSMomentaryPushInButton];
  }
  SetUpButtonCell(cell, part, states, zoomed_rect, zoom_factor);
  return cell;
}

const IntSize* LayoutThemeMac::StepperSizes() {
  static const IntSize kSizes[3] = {IntSize(19, 27), IntSize(15, 22),
                                    IntSize(13, 15)};
  return kSizes;
}

}  // namespace blink

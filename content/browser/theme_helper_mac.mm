// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/theme_helper_mac.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

using content::RenderProcessHost;
using content::RenderProcessHostImpl;
using content::ThemeHelperMac;

namespace {

void FillScrollbarThemeParams(
    content::mojom::UpdateScrollbarThemeParams* params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
  [defaults synchronize];

  // NSScrollerButtonDelay and NSScrollerButtonPeriod are no longer initialized
  // in +[NSApplication _initializeRegisteredDefaults] as of 10.15. Their values
  // still seem to affect behavior, but their use is logged as an "unusual app
  // config", so it's not clear how much longer they'll be implemented.
  params->has_initial_button_delay =
      [defaults objectForKey:@"NSScrollerButtonDelay"] != nil;
  params->initial_button_delay =
      [defaults floatForKey:@"NSScrollerButtonDelay"];
  params->has_autoscroll_button_delay =
      [defaults objectForKey:@"NSScrollerButtonPeriod"] != nil;
  params->autoscroll_button_delay =
      [defaults floatForKey:@"NSScrollerButtonPeriod"];
  params->jump_on_track_click =
      [defaults boolForKey:@"AppleScrollerPagingBehavior"];
  params->preferred_scroller_style =
      static_cast<blink::ScrollerStyle>([NSScroller preferredScrollerStyle]);

  id rubber_band_value = [defaults objectForKey:@"NSScrollViewRubberbanding"];
  params->scroll_view_rubber_banding =
      rubber_band_value ? [rubber_band_value boolValue] : YES;
}

void SendSystemColorsChangedMessage(content::mojom::Renderer* renderer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
  [defaults synchronize];

  renderer->OnSystemColorsChanged(
      [[defaults stringForKey:@"AppleAquaColorVariant"] intValue]);
}

SkColor NSColorToSkColor(NSColor* color) {
  NSColor* color_in_color_space =
      [color colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
  if (color_in_color_space) {
    // Use nextafter() to avoid rounding colors in a way that could be off-by-
    // one. See https://bugs.webkit.org/show_bug.cgi?id=6129.
    static const double kScaleFactor = nextafter(256.0, 0.0);
    return SkColorSetARGB(
        static_cast<int>(kScaleFactor * [color_in_color_space alphaComponent]),
        static_cast<int>(kScaleFactor * [color_in_color_space redComponent]),
        static_cast<int>(kScaleFactor * [color_in_color_space greenComponent]),
        static_cast<int>(kScaleFactor * [color_in_color_space blueComponent]));
  }

  // This conversion above can fail if the NSColor in question is an
  // NSPatternColor (as many system colors are). These colors are actually a
  // repeating pattern not just a solid color. To work around this we simply
  // draw a 1x1 image of the color and use that pixel's color. It might be
  // better to use an average of the colors in the pattern instead.
  NSBitmapImageRep* offscreen_rep =
      [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nil
                                              pixelsWide:1
                                              pixelsHigh:1
                                           bitsPerSample:8
                                         samplesPerPixel:4
                                                hasAlpha:YES
                                                isPlanar:NO
                                          colorSpaceName:NSDeviceRGBColorSpace
                                             bytesPerRow:4
                                            bitsPerPixel:32];

  {
    gfx::ScopedNSGraphicsContextSaveGState gstate;
    NSGraphicsContext.currentContext =
        [NSGraphicsContext graphicsContextWithBitmapImageRep:offscreen_rep];
    [color set];
    NSRectFill(NSMakeRect(0, 0, 1, 1));
  }

  NSUInteger pixel[4];
  [offscreen_rep getPixel:pixel atX:0 y:0];
  // This recursive call will not recurse again, because the color space
  // the second time around is NSDeviceRGBColorSpace.
  return NSColorToSkColor([NSColor colorWithDeviceRed:pixel[0] / 255.
                                                green:pixel[1] / 255.
                                                 blue:pixel[2] / 255.
                                                alpha:1.]);
}

} // namespace

@interface SystemThemeObserver : NSObject {
  base::RepeatingClosure _colorsChangedCallback;
}

- (instancetype)initWithColorsChangedCallback:
    (base::RepeatingClosure)colorsChangedCallback;
- (void)appearancePrefsChanged:(NSNotification*)notification;
- (void)behaviorPrefsChanged:(NSNotification*)notification;
- (void)notifyPrefsChangedWithRedraw:(BOOL)redraw;

@end

@implementation SystemThemeObserver

- (instancetype)initWithColorsChangedCallback:
    (base::RepeatingClosure)colorsChangedCallback {
  if (!(self = [super init])) {
    return nil;
  }

  _colorsChangedCallback = std::move(colorsChangedCallback);

  NSDistributedNotificationCenter* distributedCenter =
      NSDistributedNotificationCenter.defaultCenter;
  [distributedCenter addObserver:self
                        selector:@selector(appearancePrefsChanged:)
                            name:@"AppleAquaScrollBarVariantChanged"
                          object:nil
               suspensionBehavior:
                   NSNotificationSuspensionBehaviorDeliverImmediately];

  [distributedCenter addObserver:self
                        selector:@selector(behaviorPrefsChanged:)
                            name:@"AppleNoRedisplayAppearancePreferenceChanged"
                          object:nil
              suspensionBehavior:NSNotificationSuspensionBehaviorCoalesce];

  [distributedCenter addObserver:self
                        selector:@selector(behaviorPrefsChanged:)
                            name:@"NSScrollAnimationEnabled"
                          object:nil
              suspensionBehavior:NSNotificationSuspensionBehaviorCoalesce];

  [distributedCenter addObserver:self
                        selector:@selector(appearancePrefsChanged:)
                            name:@"AppleScrollBarVariant"
                          object:nil
              suspensionBehavior:
                  NSNotificationSuspensionBehaviorDeliverImmediately];

  [distributedCenter
             addObserver:self
                selector:@selector(behaviorPrefsChanged:)
                    name:@"NSScrollViewRubberbanding"
                  object:nil
      suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];

  // In single-process mode, renderers will catch these notifications
  // themselves and listening for them here may trigger the DCHECK in Observe().
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];

    [center addObserver:self
               selector:@selector(behaviorPrefsChanged:)
                   name:NSPreferredScrollerStyleDidChangeNotification
                 object:nil];

    [center addObserver:self
               selector:@selector(systemColorsChanged:)
                   name:NSSystemColorsDidChangeNotification
                 object:nil];
  }

  return self;
}

- (void)dealloc {
  [NSDistributedNotificationCenter.defaultCenter removeObserver:self];
}

- (void)appearancePrefsChanged:(NSNotification*)notification {
  [self notifyPrefsChangedWithRedraw:YES];
}

- (void)behaviorPrefsChanged:(NSNotification*)notification {
  [self notifyPrefsChangedWithRedraw:NO];
}

- (void)systemColorsChanged:(NSNotification*)notification {
  _colorsChangedCallback.Run();

  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd();
       it.Advance()) {
    SendSystemColorsChangedMessage(
        it.GetCurrentValue()->GetRendererInterface());
  }
}

- (void)notifyPrefsChangedWithRedraw:(BOOL)redraw {
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd();
       it.Advance()) {
    content::mojom::UpdateScrollbarThemeParamsPtr params =
        content::mojom::UpdateScrollbarThemeParams::New();
    FillScrollbarThemeParams(params.get());
    params->redraw = redraw;
    RenderProcessHostImpl* process_host =
        static_cast<RenderProcessHostImpl*>(it.GetCurrentValue());
    process_host->GetRendererInterface()->UpdateScrollbarTheme(
        std::move(params));
  }

  for (content::WebContentsImpl* web_contents :
       content::WebContentsImpl::GetAllWebContents()) {
    web_contents->OnWebPreferencesChanged();
  }
}

@end

namespace content {

struct ThemeHelperMac::ObjCStorage {
  // ObjC object that observes notifications from the system.
  SystemThemeObserver* __strong theme_observer;
};

// static
ThemeHelperMac* ThemeHelperMac::GetInstance() {
  static ThemeHelperMac* instance = new ThemeHelperMac();
  return instance;
}

base::ReadOnlySharedMemoryRegion
ThemeHelperMac::DuplicateReadOnlyColorMapRegion() {
  return read_only_color_map_.Duplicate();
}

ThemeHelperMac::ThemeHelperMac()
    : objc_storage_(std::make_unique<ObjCStorage>()) {
  // Allocate a region for the SkColor value table and map it.
  auto writable_region = base::WritableSharedMemoryRegion::Create(
      sizeof(SkColor) * blink::kMacSystemColorIDCount *
      blink::kMacSystemColorSchemeCount);
  writable_color_map_ = writable_region.Map();

  // Downgrade the region to read-only after it has been mapped.
  read_only_color_map_ = base::WritableSharedMemoryRegion::ConvertToReadOnly(
      std::move(writable_region));

  // Store the current color scheme into the table.
  LoadSystemColors();

  // Start observing for changes.
  objc_storage_->theme_observer = [[SystemThemeObserver alloc]
      initWithColorsChangedCallback:base::BindRepeating(
                                        &ThemeHelperMac::LoadSystemColors,
                                        base::Unretained(this))];
}

ThemeHelperMac::~ThemeHelperMac() = default;

void ThemeHelperMac::LoadSystemColorsForCurrentAppearance(
    base::span<SkColor> values) {
  for (size_t i = 0; i < blink::kMacSystemColorIDCount; ++i) {
    blink::MacSystemColorID color_id = static_cast<blink::MacSystemColorID>(i);
    switch (color_id) {
      case blink::MacSystemColorID::kControlAccentBlueColor: {
        NSColor* color =
            [NSColor colorWithCatalogName:@"System"
                                colorName:@"controlAccentBlueColor"];
        if (color) {
          values[i] = NSColorToSkColor(color);
        } else {
          // If the controlAccentBlueColor isn't available just set a black
          // value.
          values[i] = SK_ColorBLACK;
        }
        break;
      }
      case blink::MacSystemColorID::kControlAccentColor:
        values[i] = NSColorToSkColor(NSColor.controlAccentColor);
        break;
      case blink::MacSystemColorID::kKeyboardFocusIndicator:
        values[i] = NSColorToSkColor(NSColor.keyboardFocusIndicatorColor);
        break;
      case blink::MacSystemColorID::kSecondarySelectedControl:
        values[i] = NSColorToSkColor(
            NSColor.unemphasizedSelectedContentBackgroundColor);
        break;
      case blink::MacSystemColorID::kSelectedTextBackground:
        values[i] = NSColorToSkColor(NSColor.selectedTextBackgroundColor);
        break;
      case blink::MacSystemColorID::kCount:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
}

void ThemeHelperMac::LoadSystemColors() {
  static_assert(blink::kMacSystemColorSchemeCount == 2,
                "Light and dark color scheme system colors loaded.");
  base::span<SkColor> values = writable_color_map_.GetMemoryAsSpan<SkColor>(
      blink::kMacSystemColorIDCount * blink::kMacSystemColorSchemeCount);

  [[NSAppearance appearanceNamed:NSAppearanceNameAqua]
      performAsCurrentDrawingAppearance:^{
        LoadSystemColorsForCurrentAppearance(values.subspan(
            0, static_cast<size_t>(blink::MacSystemColorID::kCount)));
      }];
  [[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]
      performAsCurrentDrawingAppearance:^{
        LoadSystemColorsForCurrentAppearance(values.subspan(
            static_cast<size_t>(blink::MacSystemColorID::kCount),
            static_cast<size_t>(blink::MacSystemColorID::kCount)));
      }];
}

void ThemeHelperMac::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  // When a new RenderProcess is created, send it the initial preference
  // parameters.
  content::mojom::UpdateScrollbarThemeParamsPtr params =
      content::mojom::UpdateScrollbarThemeParams::New();
  FillScrollbarThemeParams(params.get());
  params->redraw = false;

  RenderProcessHostImpl* process_host =
      static_cast<content::RenderProcessHostImpl*>(host);
  content::mojom::Renderer* renderer = process_host->GetRendererInterface();
  renderer->UpdateScrollbarTheme(std::move(params));
  SendSystemColorsChangedMessage(renderer);
}

}  // namespace content

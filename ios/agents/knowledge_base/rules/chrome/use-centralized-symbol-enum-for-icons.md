---
name: ios/use-centralized-symbol-enum-for-icons
description: Load and size SF Symbols and custom vector symbols through the Symbol enum and helpers in symbols.h instead of string literals, systemImageNamed, bare point-size UIImageSymbolConfiguration, or UIGraphicsImageRenderer redraws
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: UI and Assets Best Practices
    quote: Use the `Symbol` enum and symbol helpers in `//ios/chrome/browser/shared/ui/symbols/symbols.h` (`SymbolWithPointSize()`, `SymbolTemplateWithPointSize()`, `SymbolWithConfiguration()`) for icons whenever possible.
  - path: ios/chrome/browser/shared/ui/symbols/README.md
    heading: Symbol folder
    quote: This folder is supposed to contain the custom symbols, the definition of custom and system symbols and the code that is used to work with the symbols.
---

**Rule.** Load and size SF Symbols and custom vector symbols through the
`Symbol` enum and helpers in `ios/chrome/browser/shared/ui/symbols/symbols.h`.
Register new symbols in `ios/chrome/browser/shared/ui/symbols/` instead of
defining local `NSString*` constants or calling `[UIImage systemImageNamed:]`.
Do not load vector symbols with `[UIImage imageNamed:]`, which remains valid for
non-symbol raster illustrations and banners. Size standard icons with
`SymbolWithPointSize()` or `SymbolTemplateWithPointSize()`. Use
`SymbolWithConfiguration()` only with a custom `UIImageConfiguration` that sets
explicit attributes (such as weight, scale, or text style), or with `nil` when
UIKit supplies the size (for example, in `UIMenu` or `UIKeyCommand`). Never
construct a bare `[UIImageSymbolConfiguration configurationWithPointSize:]` or
resize vector symbols by redrawing them in `UIGraphicsImageRenderer`.

**Why.** Centralizing symbol definitions in
`ios/chrome/browser/shared/ui/symbols/` prevents duplicate string constants and
keeps system and custom symbol loading consistent. `SymbolWithPointSize()` and
`SymbolTemplateWithPointSize()` preserve Chrome's default symbol weight, scale,
and vector rendering, unlike bare `configurationWithPointSize:` or
`UIGraphicsImageRenderer` redraws. See
`ios/best_practices.md#ui-and-assets-best-practices` and
`ios/chrome/browser/shared/ui/symbols/README.md#symbol-folder`.

## Examples

Bad:

```cpp
// 1. Local symbol string constants or systemImageNamed:/imageNamed: for
// symbols:
NSString* const kDeleteIcon = @"trash";
UIImage* deleteIcon = [UIImage systemImageNamed:kDeleteIcon];
UIImage* customIcon = [UIImage imageNamed:@"foo_custom_symbol"];
UIImageView* chevronView = [[UIImageView alloc]
    initWithImage:[UIImage systemImageNamed:@"chevron.down"
                          withConfiguration:customConfig]];

// 2. Building a bare point-size UIImageSymbolConfiguration:
UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
    configurationWithPointSize:kSymbolActionPointSize];
UIImage* actionIcon = SymbolWithConfiguration(SymbolFooAction, config);

// 3. Resizing vector symbols by redrawing in UIGraphicsImageRenderer:
UIImage* original = MakeSymbolMulticolor(
    SymbolWithPointSize(SymbolFooCustomLogo, kFooLogoSize));
UIGraphicsImageRenderer* renderer = [[UIGraphicsImageRenderer alloc]
    initWithSize:CGSizeMake(kFooLogoSize, kFooLogoSize)];
UIImage* logoIcon =
    [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
      [original drawInRect:CGRectMake(0, 0, kFooLogoSize, kFooLogoSize)];
    }];
```

Good:

```cpp
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

// 1. Reference centralized Symbol enums (registering new symbols under
// ios/chrome/browser/shared/ui/symbols/):
UIImage* deleteIcon =
    SymbolWithPointSize(SymbolFooDelete, kSymbolActionPointSize);
UIImage* customIcon = SymbolWithConfiguration(SymbolFooCustomSymbol, nil);
UIImageView* chevronView = [[UIImageView alloc]
    initWithImage:SymbolWithConfiguration(SymbolFooChevron, customConfig)];

// 2. Size standard icons directly via point-size helpers:
UIImage* actionIcon =
    SymbolWithPointSize(SymbolFooAction, kSymbolActionPointSize);

// 3. Size custom vector symbols directly via point-size helpers:
UIImage* logoIcon = MakeSymbolMulticolor(
    SymbolWithPointSize(SymbolFooCustomLogo, kFooLogoSize));
```

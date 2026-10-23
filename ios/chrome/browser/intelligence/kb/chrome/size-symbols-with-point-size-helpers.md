---
name: ios/size-symbols-with-point-size-helpers
description: When creating or sizing vector symbols in iOS Chrome, use standard SymbolWithPointSize helpers instead of systemImageNamed, UIGraphicsImageRenderer, or custom scale configurations
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: UI and Assets Best Practices
    quote: Use the [`Symbol`](/ios/chrome/browser/shared/ui/symbols/symbols.h) enum and symbol helpers in `//ios/chrome/browser/shared/ui/symbols/symbols.h` (`SymbolWithPointSize()`, `SymbolTemplateWithPointSize()`, `SymbolWithConfiguration()`) for icons whenever possible.
  - path: ios/chrome/browser/shared/ui/symbols/README.md
    heading: Symbol folder
    quote: This folder is supposed to contain the custom symbols, the definition of custom and system symbols and the code that is used to work with the symbols.
---

**Rule.** Size and configure SF Symbols and custom symbols through the standard
symbol helpers in `ios/chrome/browser/shared/ui/symbols/symbols.h`
(`SymbolWithPointSize()`, `SymbolTemplateWithPointSize()`, and
`SymbolWithConfiguration()` taking a `Symbol` enum value), passing the desired
point size. Do not construct raw `UIImageSymbolConfiguration` instances with
scale overrides (such as `UIImageSymbolScaleSmall`) or non-default weights, and
do not resize vector symbols by redrawing them into a `UIGraphicsImageRenderer`
context. When multi-state or recurring symbols (such as password visibility
toggles) are used across UI components, encapsulate their creation in a shared
UI utility helper rather than duplicating symbol selection across view
controllers. In standalone app extensions, where the symbol library is
unavailable and symbols are created with the extension's
`DefaultSymbolWithPointSize(NSString*, CGFloat)` helper, keep the configuration
attributes (point size, weight, and scale) identical to those used in the main
application. Choosing the symbol itself (the `Symbol` enum value instead of a
string literal or `[UIImage systemImageNamed:]`) is covered by
`ios/use-centralized-symbol-enum-for-icons`.

**Why.** In iOS Chrome, system and custom symbols are vector assets managed
through standard symbol typography wrappers. Inline `UIImageSymbolConfiguration`
objects with scale or weight overrides bypass Chrome's symbol system,
introducing visual inconsistencies in weight and scaling. Sizing symbols via
`UIGraphicsImageRenderer` creates unnecessary raster contexts, degrades vector
scalability, and adds error-prone manual rect math when symbol helpers already
handle point-size configuration natively. Centralizing recurring symbols and
matching main-app configurations in extensions ensures cohesive icon styling
across all Chrome UI surfaces. See
`ios/best_practices.md#ui-and-assets-best-practices` and
`ios/chrome/browser/shared/ui/symbols/README.md#symbol-folder`.

## Examples

Bad:

```cpp
// 1. Overriding symbol scale or weight via custom configuration:
UIImageConfiguration* config = [UIImageSymbolConfiguration
    configurationWithPointSize:kGeminiLiveSymbolPointSize
                        weight:UIImageSymbolWeightBold
                         scale:UIImageSymbolScaleSmall];
UIImage* image = SymbolWithConfiguration(SymbolWaveform, config);

// 2. Resizing vector symbols using UIGraphicsImageRenderer:
UIImage* original = MakeSymbolMulticolor(
    SymbolWithPointSize(SymbolGoogleWalletIcon, kLogoHeight));
UIGraphicsImageRenderer* renderer =
    [[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(kLogoWidth, kLogoWidth)];
UIImage* resized = [renderer imageWithActions:^(UIGraphicsImageRendererContext* ctx) {
  [original drawInRect:CGRectMake(0, 0, kLogoWidth, kLogoHeight)];
}];

// 3. In an app extension, using a symbol point size that does not match the
// main app's (18 pt vs 15 pt):
const CGFloat kSymbolActionPointSize = 18;
```

Good:

```cpp
// 1. Adjusting point size directly with the standard helpers:
UIImage* icon = SymbolWithPointSize(SymbolSettings, kSymbolPointSize);
UIImage* image =
    SymbolWithPointSize(SymbolWaveform, kGeminiLiveSymbolPointSize);

// 2. Sizing custom symbols directly via the point-size helper:
UIImage* wallet = MakeSymbolMulticolor(
    SymbolWithPointSize(SymbolGoogleWalletIcon, kGoogleWalletLogoV2Height));

// 3. In the credential provider extension: centralized recurring symbol
// creation with the point size used by the main app.
const CGFloat kSymbolActionPointSize = 15;
UIImage* GetPasswordVisibilityIcon(bool is_visible) {
  NSString* symbol_name = is_visible ? kHideActionSymbol : kShowActionSymbol;
  return DefaultSymbolWithPointSize(symbol_name, kSymbolActionPointSize);
}
```

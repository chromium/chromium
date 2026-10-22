---
name: ios/use-centralized-symbol-enum-for-icons
rubric: readability
severity: should_fix
---

**Rule.** In iOS Chrome UI code, do not hardcode SF Symbol or custom symbol
string literals at call sites, call `[UIImage systemImageNamed:]`, or define
feature-specific symbol name constants in local files. Instead, import
`#import "ios/chrome/browser/shared/ui/symbols/symbols.h"` and reference
centralized typed `Symbol` enum values from `symbol_enums.h` (such as
`SymbolChevronDown`, `SymbolTrash`, or `SymbolFolderBadgePlus`) using helper
functions like `SymbolWithPointSize`, `SymbolWithConfiguration`, or
`SymbolTemplateWithPointSize`. When introducing a new symbol, declare it in
`symbol_enums.h` and map it in `symbol_info.mm` (or define a shared `k...Symbol`
constant in standalone extensions where the symbol library is unavailable)
rather than defining ad-hoc local constants.

**Why.** Centralizing symbol definitions in
`ios/chrome/browser/shared/ui/symbols/` prevents duplicate string constants
across features, eliminates typos, and simplifies migrations and audits across
iOS SDK versions. Using typed `Symbol` enum constants enables compile-time
verification and guarantees visual consistency across custom and system symbols,
whereas calling `[UIImage systemImageNamed:]` or hardcoding string literals
bypasses Chrome's centralized symbol asset pipeline. See
`ios/best_practices.md#ui-and-assets-best-practices` and
`ios/chrome/browser/shared/ui/symbols/README.md#symbol-folder`.

## Examples

Bad:

```cpp
// Local string constant or calling systemImageNamed: directly:
NSString* const kCustomizationDeleteIcon = @"trash";

UIImage* folderIcon = [UIImage imageNamed:@"folder_badge_plus"];
UIImageView* chevronView = [[UIImageView alloc]
    initWithImage:[UIImage systemImageNamed:@"chevron.down"
                          withConfiguration:config]];
```

Good:

```cpp
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

// Reference centralized Symbol enums via helper functions:
UIImage* deleteIcon = SymbolWithPointSize(SymbolTrash, pointSize);
UIImage* folderIcon = SymbolWithConfiguration(SymbolFolderBadgePlus, nil);
UIImageView* chevronView = [[UIImageView alloc]
    initWithImage:SymbolWithConfiguration(SymbolChevronDown, config)];

// Or register new symbols in symbol_enums.h and symbol_info.mm:
// case SymbolFolderBadgePlus: return {@"folder.badge.plus", SymbolType::kSystem};
```

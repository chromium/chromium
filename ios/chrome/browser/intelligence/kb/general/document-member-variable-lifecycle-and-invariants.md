---
name: ios/document-member-variable-lifecycle-and-invariants
rubric: readability
severity: should_fix
---

**Rule.** Precede struct or class data members, Objective-C instance variables
(ivars), and properties (@property) in headers (.h) and implementation files
(.cc, .mm) with comments describing their purpose, semantic role, ownership
semantics, units of measurement, valid ranges, and valid states. Clarify what
entities ambiguous tokens, identifiers, indices, or flags refer to (such as
whether a token represents the current, parent, or main frame, or which feature
a flag controls). When a container or cache omits an entity discriminator from
its key, document the external invalidation invariant or trigger that clears it.
For non-owning pointers or references (raw_ptr<T>, raw_ref<T>, T\*, \_\_weak)
and service dependencies, observers, or bridges (std::unique_ptr,
scoped_refptr), document ownership semantics and who manages their lifecycle.
For public header properties in Objective-C interfaces (@interface) or protocols
(@protocol)—including dependencies (such as mutators, data sources, delegates,
handlers, or consumers), configuration properties, interface outlets (IBOutlet),
and boolean state flags—document their destination, lifecycle stage, usage
contract, or semantic scope (such as account-synced vs. device-local state).
When non-nullability is enforced via runtime assertions (such as CHECK),
explicitly document that the property cannot be nil (since nullability
annotations like nonnull/nullable are disallowed in ios/). For configurable
properties, document default values (e.g., 'StaticBlue400 by default.'),
replacement fallbacks, lifecycle timing requirements, side effects, and
non-dynamic appearance limitations where attached views do not re-render. For
internal state variables (such as throttling counters, cached UI elements, and
state flags), document their component state role unless completely obvious from
name and type. For transitional wrapper fields, document their temporary role
with a tracking TODO(crbug.com/<bug>). Prefer concise descriptive noun phrases
(e.g., '// Service used to...') over verbose clauses with auxiliary verbs (e.g.,
'// The Service is used to...'), avoid capitalizing common nouns mid-phrase, and
do not replace per-member comments with vague section headers (such as '//
Updatable Items.'). Comments that merely restate the member name or type are
covered by `ios/avoid-tautological-method-doc-comments`. Document stored state
and properties by what condition or attribute they represent ('Stores ...',
'Whether ...') rather than accessors ('Used to get ...'), getter-method phrasing
('Returns ...'), caller meta-phrasing ('Enables the caller to ...'), mutation
mechanics, or redundant notes on standard UIKit animation behavior.

**Why.** Undocumented or ambiguous member comments force readers to inspect
implementation details to deduce field roles, units, constraints, and ownership.
In public Objective-C headers, properties form the external API contract for
coordinators, mediators, and views. Because the iOS style guide bans nullability
annotations (nonnull/nullable) in favor of runtime checks, callers cannot rely
on compiler warnings and must be informed via header comments when a property
cannot be nil. Documenting purpose, defaults, fallbacks, lifecycle timing, and
non-dynamic limitations prevents callers from assuming properties reactively
update attached views or inspecting internal implementation files to determine
when dependencies (such as mutators or delegates) must be attached. Omitting
what entity an identifier or frame token references leads to misattribution
across execution flows, while omitting invalidation invariants from caches or
callback maps lacking entity discriminators makes intentional designs
indistinguishable from cross-entity pollution bugs. Furthermore, unannotated
non-owning pointers obscure lifetime management, missing cleanup TODOs leave
permanent technical debt, and describing state or properties with getter
phrasing ('Returns...'), caller meta-phrasing ('Enables the caller to...'), or
mutation mechanics conflates stored attributes with function execution or caller
mechanics. Concise noun phrases eliminate cognitive noise and verbose filler
while clarifying component expectations.

## Examples

Bad:

```cpp
struct ExtractionContext {
  // Bad: Ambiguous entity token and undocumented cache invalidation.
  std::optional<autofill::LocalFrameToken> frame_token;
  std::map<CGFloat, Callback> icon_callbacks;
  // Bad: Verbose clause and accessor phrasing.
  // The Service is used to get the user's sign-in status.
  raw_ptr<AuthenticationService> auth_service;
};

@interface PriceChipView : UIView
// Bad: Undocumented role, default value, and non-nil CHECK precondition.
@property(nonatomic, weak) id<PriceChipMutator> mutator;
@property(nonatomic) UIColor* barColor;
// Bad: Caller meta-phrasing ('Enables the caller...') and getter phrasing ('Returns...').
// Enables the caller to customize the font.
@property(nonatomic, strong) UIFont* priceFont;
// Returns a recommended title to display.
@property(nonatomic, readonly) NSString* dialogTitle;
@end
```

Good:

```cpp
struct ExtractionContext {
  // The frame token of the current frame being extracted.
  std::optional<autofill::LocalFrameToken> frame_token;
  // Map of icon sizes to callbacks; cleared when default search engine changes.
  std::map<CGFloat, Callback> icon_callbacks;
  // Service used to check the user's sign-in status. Managed by KeyedServiceFactory.
  raw_ptr<AuthenticationService> auth_service;
};

@interface PriceChipView : UIView
// Mutator for receiving user interaction events from the price chip view.
@property(nonatomic, weak) id<PriceChipMutator> mutator;
// The color of the bar for empty thumbnails; cannot be nil.
@property(nonatomic) UIColor* barColor;
// Changing this after the view is attached does not update it. Default is UIFontTextStyleFootnote.
@property(nonatomic, strong) UIFont* priceFont;
// Recommended title to display to the user.
@property(nonatomic, readonly) NSString* dialogTitle;
@end
```

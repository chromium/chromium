---
name: ios/encapsulate-view-configuration-before-hierarchy-insertion
description: When configuring UIKit views, set translatesAutoresizingMaskIntoConstraints to NO before addSubview:, encapsulate creation in dedicated helpers without container attachment, and avoid post-assembly mutations
rubric: design
severity: should_fix
---

**Rule.** When configuring programmatically created views with Auto Layout, set
translatesAutoresizingMaskIntoConstraints = NO before adding the view to its
superview hierarchy with addSubview: or addArrangedSubview:. Avoid adding views
to a superview before disabling autoresizing mask constraint translation.
Additionally, when creating UIKit subviews using dedicated factory or creation
helper methods (such as -configured<Component>, -create<View>, -build<View>, or
-make<View>), encapsulate all view-specific configuration, visual styling (such
as background colors, tints, or fonts), accessibility attributes, Auto Layout
flags (translatesAutoresizingMaskIntoConstraints = NO), and intrinsic dimension
constraints (such as fixed widthAnchor or heightAnchor constraints) within the
helper itself. Do not attach the subview to a parent container (addSubview:,
addArrangedSubview:) or configure parent-relative layout constraints inside the
creation helper; return the fully configured subview and let the caller (such as
-viewDidLoad or -setupSubviews) assemble the view hierarchy and install
inter-view positioning constraints. Conversely, during initial setup do not
finish configuring a child view's static properties, styling, or Auto Layout
flags after inserting it into the view hierarchy or inside caller layout loops;
complete that configuration in the creation helper. This applies to initial
configuration only: later state-driven updates (such as a consumer
`-set<State>:` method toggling a child's visibility, text, or tint) are expected
and go through explicit instance references. Never configure child view styling
or appearance by iterating over container `subviews` or `arrangedSubviews` after
assembly; if dynamic updates are required across children, update them via
explicit instance references rather than inspecting or casting container
subviews.

**Why.** Setting translatesAutoresizingMaskIntoConstraints = NO prior to
addSubview: (or addArrangedSubview:) adheres to standard UIKit idioms and
prevents UIKit from generating and activating implicit autoresizing mask
constraints when the view is attached to the hierarchy. Encapsulating view
configuration, styling, and intrinsic sizing inside dedicated creation helpers
enforces separation of concerns, guarantees consistent initialization across all
instances, and keeps view hierarchy assembly methods focused strictly on
structural layout. Attaching views or adding parent constraints inside creation
helpers introduces unwanted side effects and duplicates hierarchy logic across
conditional branches. Conversely, scattering initial child configuration after
insertion or iterating over subviews and arrangedSubviews couples styling logic
to fragile view hierarchy ordering, causes layout churn, and risks runtime cast
errors when spacer or container views are introduced.

## Examples

Bad:

```cpp
// Anti-pattern 1: Adding to superview before disabling autoresizing mask translation.
[self addSubview:datePicker];
datePicker.translatesAutoresizingMaskIntoConstraints = NO;

// Anti-pattern 2: Helper attaches view to container and adds constraints.
- (UIView*)configuredBackground {
  UIView* background = [[UIView alloc] init];
  [_container addSubview:background];
  AddSameConstraints(_container, background);
  return background;
}

// Anti-pattern 3: Finishing initial setup or iterating subviews post-insertion.
UIView* bar = [self createBar];
[_stackView addArrangedSubview:bar];
[bar.heightAnchor constraintEqualToConstant:kBarHeight].active = YES;
for (UIButton* button in _toolbarStackView.subviews) {
  button.backgroundColor = tintColor;
}
```

Good:

```cpp
// Pattern 1: Set translatesAutoresizingMaskIntoConstraints = NO before addSubview:.
datePicker.translatesAutoresizingMaskIntoConstraints = NO;
[self addSubview:datePicker];

// Pattern 2: Creation helpers encapsulate styling, flags, and intrinsic sizing.
- (UIView*)configuredBackground {
  UIView* background = [[UIView alloc] init];
  background.translatesAutoresizingMaskIntoConstraints = NO;
  return background;
}
- (UIView*)createBar {
  UIView* bar = [[UIView alloc] init];
  bar.translatesAutoresizingMaskIntoConstraints = NO;
  [bar.heightAnchor constraintEqualToConstant:kBarHeight].active = YES;
  return bar;
}

// Pattern 3: Caller assembles hierarchy and inter-view constraints directly.
_background = [self configuredBackground];
[_container addSubview:_background];
AddSameConstraints(_container, _background);
[_stackView addArrangedSubview:[self createBar]];
```

---
name: ios/lazily-instantiate-optional-subviews
description: When configuring optional or state-dependent subviews in UIKit views or cells, instantiate and add subviews lazily on demand rather than unconditionally in initializers
rubric: design
severity: nit
---

**Rule.** When configuring table/collection cells or custom
`UIContentConfiguration` content views with rarely displayed non-stack accessory
or overlay subviews (such as shortcut badges, status indicators, or error
overlays), instantiate those subviews and activate their layout constraints
lazily when the state is active rather than allocating them unconditionally in
designated initializers (`-init`, `-initWithFrame:`). This does not apply to
standard `-init` view hierarchy setup or `UIStackView` arranged subviews whose
visual order depends on creation time. If a lazy subview has already been
created and the state is subsequently deactivated, hide the subview (via
`.hidden = YES` or `.alpha = 0`) instead of recreating or destroying it.

**Why.** Unconditionally allocating rarely displayed accessory subviews, loading
their images or symbols, and activating their Auto Layout constraints during
cell or content-view initialization incurs unnecessary heap allocations,
increases view hierarchy depth, and adds layout pass overhead for elements that
are seldom shown. Lazily instantiating them on first use keeps common-case cell
initialization lightweight and preserves scrolling performance.

## Examples

Bad:

```cpp
- (instancetype)initWithContentView:(UIView*)contentView
                         isShortcut:(BOOL)isShortcut {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _contentView = contentView;
    [self addSubview:_contentView];

    // Bad: Unconditionally creates subview and constraints even when isShortcut is NO.
    _shortcutImageView = [[UIImageView alloc] initWithImage:shortcutImage];
    _shortcutImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [_contentView addSubview:_shortcutImageView];
    [NSLayoutConstraint activateConstraints:@[ ... ]];
    _shortcutImageView.hidden = !isShortcut;
  }
  return self;
}
```

Good:

```cpp
- (instancetype)initWithContentView:(UIView*)contentView
                         isShortcut:(BOOL)isShortcut {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _contentView = contentView;
    [self addSubview:_contentView];
    _isShortcut = isShortcut;
    [self updateShortcutImageView];
  }
  return self;
}

- (void)updateShortcutImageView {
  if (!_isShortcut) {
    _shortcutImageView.hidden = YES;
    return;
  }
  // Good: Lazily instantiate subview and constraints only when first displayed.
  if (!_shortcutImageView) {
    _shortcutImageView = [[UIImageView alloc] initWithImage:shortcutImage];
    _shortcutImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [_contentView addSubview:_shortcutImageView];
    [NSLayoutConstraint activateConstraints:@[ ... ]];
  }
  _shortcutImageView.hidden = NO;
}
```

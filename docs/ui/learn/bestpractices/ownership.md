# Best practice: ownership

In the common case, a `View` instance lives within a hierarchy of `View`s, up to
a `RootView` object that is owned by a `Widget`. Calling `AddChildView<>()`
typically passes ownership of the child to the parent, while
`RemoveChildView[T<>]()` gives ownership back to the caller. While other
ownership patterns exist, newly-added code should use this one.

[TOC]

## Lifetime basics

Accordingly, the best practices for ownership and lifetime management in Views
code are similar to those in the rest of Chromium, but include some details
specific to the Views APIs.

* **[Avoid bare new.](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++-dos-and-donts.md#use-and-instead-of-bare)**
  Use `std::make_unique<>()` when creating objects, and distinguish owning and
  non-owning uses by [using smart and raw pointer types](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md#object-ownership-and-calling-conventions).
* **Use [the unique\_ptr<> version of View::AddChildView<>()](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h;l=404;drc=7cba41605a8489bace83f380760486638a2a8a4a).**
  This clearly conveys ownership transfer from the caller to the parent `View`.
  It also `DCHECK()`s that `set_owned_by_client()` has not been called,
  preventing double-ownership.

|||---|||

#####

**Avoid**

Typical code from [`dice_bubble_sync_promo_view.cc`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.cc;l=49;drc=459f2d16a6592b1290a1c2197231d46f48f49af4)
using bare `new`:

#####

**Best practice**

Rewriting using the `unique_ptr<>` version of `AddChildView<>()` is shorter and
safer:

|||---|||

|||---|||

#####

``` cpp
...
views::Label* title =
    new views::Label(
        title_text, views::style::CONTEXT_DIALOG_BODY_TEXT,
        text_style);

title->SetHorizontalAlignment(
    gfx::HorizontalAlignment::ALIGN_LEFT);
title->SetMultiLine(true);
AddChildView(title);
...
signin_button_view_ =
    new DiceSigninButtonView(
        account, account_icon, this,
        /*use_account_name_as_title=*/true);
AddChildView(signin_button_view_);
...
```

#####

``` cpp
...
auto* title =
    AddChildView(
        std::make_unique<views::Label>(
            title_text, views::style::CONTEXT_DIALOG_BODY_TEXT,
            text_style));
title->SetHorizontalAlignment(
    gfx::HorizontalAlignment::ALIGN_LEFT);
title->SetMultiLine(true);

...
signin_button_view_ =
    AddChildView(
        std::make_unique<DiceSigninButtonView>(
            account, account_icon, this,
            /*use_account_name_as_title=*/true));
...
```

|||---|||

## Avoid View::set_owned_by_client()

The [`View::set_owned_by_client()`](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h;l=390;drc=7cba41605a8489bace83f380760486638a2a8a4a)
flag means that a `View` is owned by something other than its parent `View`.
This **method is deprecated** (and will eventually be removed) since it results
in APIs that are easy to misuse, code where ownership is unclear, and a higher
likelihood of bugs. Needing this flag may be a signal that the `View` subclass
in question is too heavyweight, and refactoring using MVC or a similar paradigm
would allow a long-lived "model" or "controller" with more-transient "view"s.

|||---|||

#####

**Avoid**

Code in [`time_view.{h,cc}`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/time/time_view.h;l=35;drc=7d8bc7f807a433e6a127806e991fe780aa27ce77)
that uses `set_owned_by_client()` to have non-parented `View`s, so it can swap
layouts without recreating the children:

#####

**Best practice**

Rewriting using subclasses to encapsulate layout allows the parent to merely
adjust visibility:

|||---|||

|||---|||

#####

``` cpp
class ASH_EXPORT TimeView : public ActionableView,
                            public ClockObserver {
  ...
 private:



  std::unique_ptr<views::Label> horizontal_label_;
  ...
  std::unique_ptr<views::Label> vertical_label_hours_;
  std::unique_ptr<views::Label> vertical_label_minutes_;
  ...
};

void TimeView::SetupLabels() {
  horizontal_label_.reset(new views::Label());
  SetupLabel(horizontal_label_.get());
  vertical_label_hours_.reset(new views::Label());
  SetupLabel(vertical_label_hours_.get());
  vertical_label_minutes_.reset(new views::Label());
  SetupLabel(vertical_label_minutes_.get());
  ...
}

void TimeView::SetupLabel(views::Label* label) {
  label->set_owned_by_client();
  ...
}











void TimeView::UpdateClockLayout(
    ClockLayout clock_layout) {
  ...
  if (clock_layout == ClockLayout::HORIZONTAL_CLOCK) {
    RemoveChildView(vertical_label_hours_.get());
    RemoveChildView(vertical_label_minutes_.get());
    SetLayoutManager(
        std::make_unique<views::FillLayout>());
    AddChildView(horizontal_label_.get());
  } else {
    RemoveChildView(horizontal_label_.get());
    // Remove the current layout manager since it could
    // be the FillLayout which only allows one child.
    SetLayoutManager(nullptr);
    // Pre-add the children since ownership is being
    // retained by this.
    AddChildView(vertical_label_hours_.get());
    AddChildView(vertical_label_minutes_.get());
    views::GridLayout* layout =
        SetLayoutManager(
            std::make_unique<views::GridLayout>());
    ...
  }
  DeprecatedLayoutImmediately();
}
```

#####

``` cpp
class ASH_EXPORT TimeView : public ActionableView,
                            public ClockObserver {
  ...
 private:
  class HorizontalLabelView;
  class VerticalLabelView;
  ...
  HorizontalLabelView* horizontal_label_;
  VerticalLabelView* vertical_label_;
  ...
};



TimeView::HorizontalLabelView::HorizontalLabelView() {
  SetLayoutManager(
      std::make_unique<views::FillLayout>());
  ...
}

TimeView::VerticalLabelView::VerticalLabelView() {
  views::GridLayout* layout =
        SetLayoutManager(
            std::make_unique<views::GridLayout>());
  ...
}

void TimeView::TimeView(ClockLayout clock_layout,
                        ClockModel* model) {
  ...
  horizontal_label_ =
     AddChildView(
        std::make_unique<HorizontalLabelView>());
  vertical_label_ =
     AddChildView(
        std::make_unique<VerticalLabelView>());
  ...
}

void TimeView::UpdateClockLayout(
    ClockLayout clock_layout) {
  ...
  const bool is_horizontal =
      clock_layout == ClockLayout::HORIZONTAL_CLOCK;
  horizontal_label_->SetVisible(is_horizontal);
  vertical_label_->SetVisible(!is_horizontal);
  DeprecatedLayoutImmediately();
}
















```

|||---|||

## Avoid refcounting and WeakPtrs

Refcounting and `WeakPtr`s may also be indicators that a `View` is doing more
than merely displaying UI. Views objects should only handle UI. Refcounting and
`WeakPtr` needs should generally be handled by helper objects.

|||---|||

#####

**Avoid**

Old code in `cast_dialog_no_sinks_view.{h,cc}` that used weak pointers to
`PostDelayedTask()` to itself:

#####

**Best practice**

[Current version](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/media_router/cast_dialog_no_sinks_view.h;l=20;drc=2a147d67e51e67144257d3a405e3aafec3827513)
eliminates lifetime concerns by using a `OneShotTimer`, which is canceled when
destroyed:

|||---|||

|||---|||

#####

``` cpp
class CastDialogNoSinksView ... {
  ...
 private:
  base::WeakPtrFactory<CastDialogNoSinksView>
      weak_factory_{this};
  ...
};

CastDialogNoSinksView::CastDialogNoSinksView(
    Profile* profile) : profile_(profile) {
  ...
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &CastDialogNoSinksView::ShowHelpIconView,
          weak_factory_.GetWeakPtr()),
      kSearchWaitTime);
}
```

#####

``` cpp
class CastDialogNoSinksView ... {
  ...
 private:
  base::OneShotTimer timer_;
  ...
};


CastDialogNoSinksView::CastDialogNoSinksView(
    Profile* profile) : profile_(profile) {
  ...
  timer_.Start(
      FROM_HERE, kSearchWaitTime,
      base::BindOnce(
          &CastDialogNoSinksView::SetHelpIconView,
          base::Unretained(this)));
}

```

|||---|||

## Use View::RemoveChildViewT<>()

[`View::RemoveChildViewT<>()`](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h;l=443;drc=7cba41605a8489bace83f380760486638a2a8a4a)
**clearly conveys ownership transfer** from the parent `View` to the caller.
Callers who wish to delete a `View` can simply ignore the return argument. This
is preferable to calling `RemoveChildView()` and deleting the raw pointer
(cumbersome and error-prone), calling `RemoveChildView()` without ever deleting
the pointer (leaks the `View`), or simply deleting a pointer to a still-parented
`View` (will work today, but is semantically incorrect and may be removed in the
future).

|||---|||

#####

**Avoid**

Typical code in [`network_list_view.cc`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_list_view.cc;l=296;drc=4867a6ce87c51024259259baad08b0ba8ae030a5)
which manually deletes a child after removing it:

#####

**Best practice**

Rewriting using `RemoveChildViewT<>()` is shorter and safer:

|||---|||

|||---|||

#####

``` cpp
  ... if (mobile_header_view_) {
    scroll_content()->RemoveChildView(
        mobile_header_view_);
    delete mobile_header_view_;
    mobile_header_view_ = nullptr;
    needs_relayout_ = true;
  }
  ...
```

#####

``` cpp
  ... if (mobile_header_view_) {
    scroll_content()->RemoveChildViewT(
        mobile_header_view_);

    mobile_header_view_ = nullptr;
    needs_relayout_ = true;
  }
  ...
```

|||---|||

## Prefer scoping objects

**Prefer scoping objects to paired Add/Remove-type calls**. For example, use
a [`base::ScopedObservation<>`](https://source.chromium.org/chromium/chromium/src/+/main:base/scoped_observation.h;l=49;drc=12993351a415619a445a836ca8a94a6310569dbd)
instead of directly calling [`View::AddObserver()`](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h;l=1346;drc=7cba41605a8489bace83f380760486638a2a8a4a)
and [`RemoveObserver()`](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h;l=1347;drc=7cba41605a8489bace83f380760486638a2a8a4a).
Such objects reduce the likelihood of use-after-free.

|||---|||

#####

**Avoid**

Typical code in [`avatar_toolbar_button.cc`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/profiles/avatar_toolbar_button.cc;l=50;drc=f81f0d5d17c037c5866b8808322931f313b796e1)
that uses paired add/remove calls:

#####

**Best practice**

Rewriting using `base::ScopedObservation<>` eliminates the destructor body entirely:

|||---|||

|||---|||

#####

``` cpp










AvatarToolbarButton::AvatarToolbarButton(
    Browser* browser, ToolbarIconContainerView* parent)
    : browser_(browser), parent_(parent) {
  ...
  if (parent_)
    parent_->AddObserver(this);
}

AvatarToolbarButton::~AvatarToolbarButton() {
  if (parent_)
    parent_->RemoveObserver(this);
}
```

#####

``` cpp
class AvatarToolbarButton
    : public ToolbarButton,
      public ToolbarIconContainerView::Observer {
  ...
 private:
  base::ScopedObservation<AvatarToolbarButton,
                 ToolbarIconContainerView::Observer>
      observation_{this};
};

AvatarToolbarButton::AvatarToolbarButton(
    Browser* browser, ToolbarIconContainerView* parent)
    : browser_(browser), parent_(parent) {
  ...
  if (parent_)
    observation_.Observe(parent_);
}

AvatarToolbarButton::~AvatarToolbarButton() = default;



```

|||---|||

## Keep lifetimes fully nested

For objects you own, **destroy in the reverse order you created,** so lifetimes
are nested rather than partially-overlapping. This can also reduce the
likelihood of use-after-free, usually by enabling code to make simplifying
assumptions (e.g. that an observed object always outlives `this`).

|||---|||

####

**Avoid**

Old code in `widget_interactive_uitest.cc` that destroys in the same order as
creation:

#####

**Best practice**

[Current version](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/widget/widget_interactive_uitest.cc;l=492;drc=cbe5dca6cb1e1511c82776370a964d99cbf5205d)
uses scoping objects that are simpler and safer:

|||---|||

|||---|||

#####

``` cpp
TEST_F(WidgetTestInteractive,
       ViewFocusOnWidgetActivationChanges) {
  Widget* widget1 = CreateTopLevelPlatformWidget();
  ...

  Widget* widget2 = CreateTopLevelPlatformWidget();
  ...
  widget1->CloseNow();
  widget2->CloseNow();
}
```

#####

``` cpp
TEST_F(WidgetTestInteractive,
       ViewFocusOnWidgetActivationChanges) {
  WidgetAutoclosePtr widget1(
      CreateTopLevelPlatformWidget());
  ...
  WidgetAutoclosePtr widget2(
      CreateTopLevelPlatformWidget());
  ...
}

```

|||---|||

## Only use Views objects on the UI thread

**Always use `View`s on the main (UI) thread.** Like most Chromium code, the
Views toolkit is [not thread-safe](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h;l=170;drc=7cba41605a8489bace83f380760486638a2a8a4a).

## Add child Views in a View's constructor

In most cases, **add child `View`s in a `View`'s constructor.** From an
ownership perspective, doing so is safe even though the `View` is not yet in a
`Widget`; if the `View` is destroyed before ever being placed in a `Widget`, it
will still destroy its child `View`s. Child `View`s may need to be added at
other times (e.g. in [`AddedToWidget()`](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h;l=1439;drc=7cba41605a8489bace83f380760486638a2a8a4a)
or [`OnThemeChanged()`](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h;l=1545;drc=7cba41605a8489bace83f380760486638a2a8a4a),
if constructing the `View` requires a color; or lazily, if creation is expensive
or a `View` is not always needed); however, do not copy any existing code that
adds child `View`s in a [`ViewHierarchyChanged()`](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h;l=1422;drc=7cba41605a8489bace83f380760486638a2a8a4a)
override, as such code is usually an artifact of misunderstanding the Views
ownership model.

|||---|||

#####

**Avoid**

Typical code in [`native_app_window_views.cc`](https://source.chromium.org/chromium/chromium/src/+/main:extensions/components/native_app_window/native_app_window_views.cc;l=285;drc=d66100ebd950c39b80cfd1c72cec618b33eb3491)
that sets up a child `View` in `ViewHierarchyChanged()`:

#####

**Best practice**

Rewriting to do this at construction/`Init()` makes |`web_view_`|'s lifetime
easier to reason about:

|||---|||

|||---|||

#####

``` cpp
void NativeAppWindowViews::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this) {
    DCHECK(!web_view_);
    web_view_ =
        AddChildView(
            std::make_unique<views::WebView>(nullptr));







    web_view_->SetWebContents(
        app_window_->web_contents());
  }
}
```

#####

``` cpp


NativeAppWindowViews::NativeAppWindowViews() {
  ...
  web_view_ =
      AddChildView(
          std::make_unique<views::WebView>(nullptr));
}

void NativeAppWindowViews::Init(
    extensions::AppWindow* app_window,
    const extensions::AppWindow::CreateParams&
        create_params) {
  app_window_ = app_window;
  web_view_->SetWebContents(
      app_window_->web_contents());
  ...
}
```

|||---|||

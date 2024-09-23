# Best practice: layout

The most important principles when working with Views layout are abstraction
and encapsulation. The goal of the guidance here is to split the broad work of
"layout" into many discrete pieces, each of which can be easily understood,
tested, and changed in isolation. Compliant code should be more readable,
performant, and maintainable; more adaptable to future modifications; and more
stylistically consistent with other UI elements both now and in the future.

[TOC]

## Express layout values logically

Both in mocks and code, **layout values should be described in functional terms
that conform to the relevant specs** (particularly the
[Material Refresh][] and older [Harmony][] specs). Rather than a mock saying
a dialog title is "15 pt high", it should say it is the "Title 1" or
"DIALOG\_TITLE" style; two non-interacting controls on the same line should not
be separated by "16 dip" but by [`DISTANCE_UNRELATED_CONTROL_HORIZONTAL`][].
Designers and engineers can reference a [dictionary][] to agree on common
terminology. Don't simply back-figure the constant to use based on what
produces the same value as the mock, as future design system changes will
cause subtle and confusing bugs. Similarly, don't hardcode designer-provided
values that aren't currently present in Chrome, as the semantics of such
one-off values are unclear and will cause maintenance problems.
Work with the Toolkit and UX teams to modify the design and overall spec so
the desired results can be achieved in a centralized way.

Note: the concept in this section is general, but the linked specs are
**Googlers Only**.

[Material Refresh]: https://docs.google.com/presentation/d/1EO7TOpIMJ7QHjaTVw9St-q6naKwtXX2TwzMirG5EsKY/edit#slide=id.g3232c09376_6_794
[Harmony]: https://folio.googleplex.com/chrome-ux-specs-and-sources/Chrome%20browser%20%28MD%29/Secondary%20UI%20Previews%20and%20specs%20%28exports%29/Spec
[`DISTANCE_UNRELATED_CONTROL_HORIZONTAL`]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/chrome_layout_provider.h;l=56;drc=ec62c9ac3ef71a7014e27c5d2cf98917a89e3524
[dictionary]: http://go/DesktopDictionary

## Obtain layout values from provider objects

Once layout styles and values are expressed functionally, **the exact values
should be obtained from relevant provider objects, not computed directly.**
Code should not have any magic numbers for sizes, positions, elevations, and
the like; this includes transformations like scaling values up or down by a
fraction, or tweaking a provided value by a few DIPs. The most commonly used
provider object is the [`LayoutProvider`][] (or its `chrome`/-side extension,
[`ChromeLayoutProvider`][]); `View`s can obtain the global instance via a
relevant [`Get()`][] method and ask it for relevant [distances][], [insets][],
[corner radii][], [shadow elevations][], and the like. For text-setting
controls like [`Label`][], the [`TypographyProvider`][] (or its
`chrome`/-side extension, [`ChromeTypographyProvider`]),
usually accessed via [global helper functions][], can provide appropriate
[fonts][], [colors][], and [line heights][]. Most `View`s should not use these
directly, but use `Label` and other such controls, providing the appropriate
[context][] and [style][].

[`LayoutProvider`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/layout_provider.h;l=129;drc=f5df5da5753795298349b2dd6325e2c5e6e13e38
[`ChromeLayoutProvider`]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/chrome_layout_provider.h;drc=ec62c9ac3ef71a7014e27c5d2cf98917a89e3524;l=76
[`Get()`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/layout_provider.h;drc=f5df5da5753795298349b2dd6325e2c5e6e13e38;l=135
[distances]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/layout_provider.h;drc=f5df5da5753795298349b2dd6325e2c5e6e13e38;l=148
[insets]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/layout_provider.h;drc=f5df5da5753795298349b2dd6325e2c5e6e13e38;l=144
[corner radii]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/layout_provider.h;drc=f5df5da5753795298349b2dd6325e2c5e6e13e38;l=168
[shadow elevations]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/layout_provider.h;drc=f5df5da5753795298349b2dd6325e2c5e6e13e38;l=172
[`Label`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/controls/label.h;l=30;drc=59135b4042aa469752899e8e4bf2a0a81d3d320c
[`TypographyProvider`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/style/typography_provider.h;l=22;drc=b5e29e075e814ed41e6727c281b69f797d8a1e10
[`ChromeTypographyProvider`]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/chrome_typography_provider.h;l=13;drc=a7ee000c95842e2dce6397ca36926924f4cb322b
[global helper functions]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/style/typography.h;l=109;drc=8f7db479018a99e5906876954de93ae6d23bee58
[fonts]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/style/typography_provider.h;l=28;drc=b5e29e075e814ed41e6727c281b69f797d8a1e10
[colors]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/style/typography_provider.h;l=32;drc=b5e29e075e814ed41e6727c281b69f797d8a1e10
[line heights]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/style/typography_provider.h;l=37;drc=b5e29e075e814ed41e6727c281b69f797d8a1e10
[context]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/style/typography.h;l=23;drc=8f7db479018a99e5906876954de93ae6d23bee58
[style]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/style/typography.h;l=67;drc=8f7db479018a99e5906876954de93ae6d23bee58

|||---|||

#####

**Avoid**

[Current code][1] uses file scoped hard-coded padding values for its layout
constants.

[1]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/subtle_notification_view.cc;l=142;drc=787d0aacc071674dc83f6059072d15f8cfffbf84

#####

**Best practice**

A better approach would be to use layout constants sourced from the
[`ChromeLayoutProvider`][].

|||---|||

|||---|||

#####

``` cpp
namespace {
// Space between the site info label.
const int kMiddlePaddingPx = 30;

const int kOuterPaddingHorizPx = 40;
const int kOuterPaddingVertPx = 8;
} // namespace

SubtleNotificationView::SubtleNotificationView()
    : instruction_view_(nullptr) {
  ...
  instruction_view_ =
      new InstructionView(std::u16string());

  int outer_padding_horiz = kOuterPaddingHorizPx;
  int outer_padding_vert = kOuterPaddingVertPx;
  AddChildView(instruction_view_);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(outer_padding_vert,
                  outer_padding_horiz),
      kMiddlePaddingPx));
}
```

#####

``` cpp








SubtleNotificationView::SubtleNotificationView()
    : instruction_view_(nullptr) {
  ...
  AddChildView(std::make_unique<InstructionView>(
      std::u16string()));

  const gfx::Insets kDialogInsets =
      ChromeLayoutProvider::Get()->GetInsetsMetric(
      views::INSETS_DIALOG);
  const int kHorizontalPadding =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      kDialogInsets, kHorizontalPadding));
}
```

|||---|||


## Use hierarchy liberally

While not a layout-specific tactic, it simplifies many layout issues
**to break a high-level UI construct into a hierarchy of `View`s**,
with as many levels as necessary to make each `View` as simple as possible.
In such hierarchies, most non-leaf `View`s will be nameless "containers" that
simply size or group their immediate children, perhaps with padding between
them or a margin around the outside. Each such `View` is easy to lay out,
and you can later combine or factor out pieces of the hierarchy as appropriate,
including adding helpers for common Material Design idioms to the core toolkit.


## Use LayoutManagers

**Avoid overriding [`Layout()`][] to programmatically lay out children.**
In nearly all cases, the built-in [`LayoutManager`][]s can achieve the desired
layout, and do so in a declarative rather than imperative fashion. The
resulting code is often simpler and easier to understand. Writing a [bespoke
`LayoutManager`][] is also possible, but less common.

[`Layout()`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h;l=730;drc=f5df5da5753795298349b2dd6325e2c5e6e13e38
[`LayoutManager`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/layout_manager.h;l=33;drc=f5df5da5753795298349b2dd6325e2c5e6e13e38
[bespoke `LayoutManager`]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/try_chrome_dialog_win/button_layout.h;l=30;drc=f5df5da5753795298349b2dd6325e2c5e6e13e38

|||---|||

#####

**Avoid**

The following old code used Layout() to have its label text fill the dialog.

#####

**Best practice**

[Current code][2] uses a [FillLayout][] to achieve the same result.

[2]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/relaunch_notification/relaunch_required_dialog_view.cc;l=91;drc=1ec33e7c19e2d63b3f918df115c12f77f419645b
[FillLayout]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/fill_layout.h

|||---|||

|||---|||

#####

``` cpp
int RelaunchRequiredDialogView::GetHeightForWidth(
    int width) const {
  const gfx::Insets insets = GetInsets();
  return body_label_->GetHeightForWidth(
      width - insets.width()) + insets.height();
}

void RelaunchRequiredDialogView::Layout(PassKey) {
  body_label_->SetBoundsRect(GetContentsBounds());
}
```

#####

``` cpp
RelaunchRequiredDialogView::RelaunchRequiredDialogView(
    base::Time deadline,
    base::RepeatingClosure on_accept)
    : ...{
  SetLayoutManager(
      std::make_unique<views::FillLayout>());
  ...
}


```

|||---|||


## Prefer intrinsic constraints to extrinsic computation

Where possible, **express the desired outcome of layout in terms of intrinsic
constraints for each `View`,** instead of trying to conditionally compute
the desired output metrics. For example, using a [`ClassProperty`][]
to set each child's [margins][] is less error-prone than trying to
conditionally add padding `View`s between children. When coupled with
[margin collapsing][] and [internal padding][], it's possible to do things
like use [different padding amounts between different children][]
or visually align elements without manually computing offsets.
Such manual computation is prone to bugs if someone changes a size, padding
value, or child order in one place without also updating related computations
elsewhere.

[`ClassProperty`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/base/class_property.h;l=55;drc=f5df5da5753795298349b2dd6325e2c5e6e13e38
[margins]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view_class_properties.h;l=30;drc=1449b8c60358c4cdea1722e4c1e8079bd1b5f306
[margin collapsing]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/flex_layout.h;l=87;drc=62bf27aca5418212ceadd8daf9188d2aa437bfcc
[internal padding]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view_class_properties.h;l=40;drc=1449b8c60358c4cdea1722e4c1e8079bd1b5f306
[different padding amounts between different children]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/toolbar/toolbar_view.cc;l=974;drc=34a8c4215229379ced3586125399c7ad3c65b87f

|||---|||

#####

**Avoid**

The following is old code that calculated bubble padding through calculations
involving the control insets.

#####

**Best practice**

[Current code][3] uses a combination of margin and padding on the
ColorPickerView to ensure proper alignment.

[3]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.cc;l=89;drc=542c4c6ac89bc665807351d3fb4aca5ebddc82f8

|||---|||

|||---|||

#####

``` cpp
TabGroupEditorBubbleView::TabGroupEditorBubbleView(
    const Browser* browser,
    const tab_groups::TabGroupId& group,
    TabGroupHeader* anchor_view,
    std::optional<gfx::Rect> anchor_rect,
    bool stop_context_menu_propagation)
    : ... {

  ...
  const auto* layout_provider =
      ChromeLayoutProvider::Get();
  const int horizontal_spacing =
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const int vertical_menu_spacing =
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL);

  // The vertical spacing for the non menu items within
  // the editor bubble.
  const int vertical_dialog_content_spacing = 16;










  views::View* group_modifier_container =
      AddChildView(std::make_unique<views::View>());

  gfx::Insets color_element_insets =
      ChromeLayoutProvider::Get()->GetInsetsMetric(
          views::INSETS_VECTOR_IMAGE_BUTTON);
  group_modifier_container->SetBorder(
      views::CreateEmptyBorder(
          gfx::Insets(vertical_dialog_content_spacing,
              horizontal_spacing -
                  color_element_insets.left(),
              vertical_dialog_content_spacing,
              horizontal_spacing -
                  color_element_insets.right())));
  ...
  // Add the text field for editing the title.
  views::View* title_field_container =
      group_modifier_container->AddChildView(
          std::make_unique<views::View>());
  title_field_container->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(
          0, color_element_insets.left(),
          vertical_dialog_content_spacing,
          color_element_insets.right()))
  ...
  color_selector_ =
    group_modifier_container->AddChildView(
        std::make_unique<ColorPickerView>(
            colors_, background_color(), initial_color,
            base::BindRepeating(
                &TabGroupEditorBubbleView::UpdateGroup,
                base::Unretained(this))));
  ...
}

























```

#####

``` cpp
TabGroupEditorBubbleView::TabGroupEditorBubbleView(
    const Browser* browser,
    const tab_groups::TabGroupId& group,
    views::View* anchor_view,
    std::optional<gfx::Rect> anchor_rect,
    TabGroupHeader* header_view,
    bool stop_context_menu_propagation)
    : ... {
  ...
  const auto* layout_provider =
      ChromeLayoutProvider::Get();
  const int horizontal_spacing =
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const int vertical_spacing =
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL);

  // The padding of the editing controls is adaptive,
  // to improve the hit target size and screen real
  // estate usage on touch devices.
  const int group_modifier_vertical_spacing =
      ui::TouchUiController::Get()->touch_ui() ?
          vertical_spacing / 2 : vertical_spacing;
  const gfx::Insets control_insets =
      ui::TouchUiController::Get()->touch_ui()
          ? gfx::Insets(5 * vertical_spacing / 4,
            horizontal_spacing)
          : gfx::Insets(vertical_spacing,
                        horizontal_spacing);

  views::View* group_modifier_container =
      AddChildView(std::make_unique<views::View>());
  group_modifier_container->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(
          group_modifier_vertical_spacing, 0)));

  views::FlexLayout* group_modifier_container_layout =
      group_modifier_container->SetLayoutManager(
          std::make_unique<views::FlexLayout>());
  group_modifier_container_layout
      ->SetOrientation(
          views::LayoutOrientation::kVertical)
      .SetIgnoreDefaultMainAxisMargins(true);


  // Add the text field for editing the title.
  views::View* title_field_container =
      group_modifier_container->AddChildView(
          std::make_unique<views::View>());
  title_field_container->SetBorder(
      views::CreateEmptyBorder(
          control_insets.top(), control_insets.left(),
          group_modifier_vertical_spacing,
          control_insets.right()));

  ...
  const tab_groups::TabGroupColorId initial_color_id =
      InitColorSet();
  color_selector_ =
      group_modifier_container->AddChildView(
          std::make_unique<ColorPickerView>(
              this, colors_, initial_color_id,
              base::BindRepeating(
                &TabGroupEditorBubbleView::UpdateGroup,
                base::Unretained(this))));
  color_selector_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(0, control_insets.left(), 0,
                  control_insets.right()));
  ...
}

ColorPickerView::ColorPickerView(
    const views::BubbleDialogDelegateView* bubble_view,
    const TabGroupEditorBubbleView::Colors& colors,
    tab_groups::TabGroupColorId initial_color_id,
    ColorSelectedCallback callback)
    : callback_(std::move(callback)) {
  ...
  // Set the internal padding to be equal to the
  // horizontal insets of a color picker element,
  // since that is the amount by which the color
  // picker's margins should be adjusted to make it
  // visually align with other controls.
  gfx::Insets child_insets = elements_[0]->GetInsets();
  SetProperty(views::kInternalPaddingKey,
              gfx::Insets(0, child_insets.left(), 0,
                          child_insets.right()));
}
```

|||---|||

## Use TableLayout with caution

[`TableLayout`][] is a `LayoutManager` used for tabular layouts. Much like
table-based layout in HTML, it can achieve almost any desired effect, and in
some scenarios (e.g.  creating an actual table) is the best tool. Used
indiscriminately, it can be cryptic, verbose, and error-prone. Accordingly,
**use `TableLayout` only when creating a true grid or table, not simply for
selective horizontal and vertical alignment.** For simple layouts,
[`BoxLayout`][] and [`FlexLayout`][] are better choices; for more complex
layouts, representing sections or groups hierarchically may result in simpler
inner layouts that can be nested within an overall layout.

[`TableLayout`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/table_layout.h;l=73;drc=f513afe81fca508d22153b192f1fab33e2c444fa
[`BoxLayout`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/box_layout.h;l=28;drc=5b9e43d976aca377588875fc59c5348ede02a8b5
[`FlexLayout`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/layout/flex_layout.h;l=73;drc=62bf27aca5418212ceadd8daf9188d2aa437bfcc

|||---|||

#####

**Avoid**

The following old code uses a [`TableLayout`][] to create a HoverButton with
a stacked title and subtitle flanked on by views on both sides.

#####

**Best practice**

[Current code][4] uses [`FlexLayout`][] to achieve the desired result, resulting
in clearer code.

[4]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/controls/hover_button.cc;l=129;drc=0139ceffb2f8e1f64b7c30834e57d5793e529ed7

|||---|||

|||---|||

#####

``` cpp
// Used to create the following layout
// +-----------+---------------------+----------------+
// | icon_view | title               | secondary_view |
// +-----------+---------------------+----------------+
// |           | subtitle            |                |
// +-----------+---------------------+----------------+
HoverButton::HoverButton(
    ...
    std::unique_ptr<views::View> icon_view,
    const std::u16string& title,
    const std::u16string& subtitle,
    std::unique_ptr<views::View> secondary_view,
    ...) {
  ...
  views::TableLayout* table_layout =
      SetLayoutManager(
          std::make_unique<views::TableLayout>());
  ...
  table_layout->AddColumn(
      views::LayoutAlignment::kCenter,
      views::LayoutAlignment::kCenter,
      views::TableLayout::kFixedSize,
      views::TableLayout::kUsePreferred, 0, 0);
  table_layout->AddPaddingColumn(
      views::TableLayout::kFixedSize,
      icon_label_spacing);
  table_layout->AddColumn(
      views::LayoutAlignment::kStretch,
      views::LayoutAlignment::kStretch, 1.0f,
      views::TableLayout::kUsePreferred, 0, 0);
  ...
  table_layout->AddRows(
      1, views::TableLayout::kFixedSize,
      row_height);
  icon_view_ = AddChildView(
      std::move(icon_view), 1, num_labels);
  ...
  auto title_wrapper =
      std::make_unique<SingleLineStyledLabelWrapper>(
          title);
  title_ = title_wrapper->label();
  AddChildView(std::move(title_wrapper));

  if (secondary_view) {
    table_layout->AddColumn(
        views::LayoutAlignment::kCenter,
        views::LayoutAlignment::kCenter,
        views::TableLayout::kFixedSize,
        views::TableLayout::kUsePreferred, 0, 0);
    ...
    secondary_view_ = AddChildView(
        std::move(secondary_view), 1, num_labels);
    ...
  }
  if (!subtitle.empty()) {
    table_layout->AddRows(
        1, views::TableLayout::kFixedSize,
        row_height);
    auto subtitle_label =
        std::make_unique<views::Label>(
            subtitle, views::style::CONTEXT_BUTTON,
            views::style::STYLE_SECONDARY);
    ...
    AddChildView(std::make_unique<views::View>());
    subtitle_ =
        AddChildView(std::move(subtitle_label));
  }
  ...
}




```

#####

``` cpp
// Used to create the following layout
// +-----------+---------------------+----------------+
// |           | title               |                |
// | icon_view |---------------------| secondary_view |
// |           | subtitle            |                |
// +-----------+---------------------+----------------+
HoverButton::HoverButton(
    ...
    std::unique_ptr<views::View> icon_view,
    const std::u16string& title,
    const std::u16string& subtitle,
    std::unique_ptr<views::View> secondary_view,
    ...) {
  ...
  // Set the layout manager to ignore the
  // ink_drop_container to ensure the ink drop tracks
  // the bounds of its parent.
  ink_drop_container()->SetProperty(
      views::kViewIgnoredByLayoutKey, true);

  SetLayoutManager(
      std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(
          views::LayoutAlignment::kCenter);
  ...
  icon_view_ =
    AddChildView(std::make_unique<IconWrapper>(
        std::move(icon_view), vertical_spacing))
    ->icon();

  // |label_wrapper| will hold both the title and
  // subtitle if it exists.
  auto label_wrapper = std::make_unique<views::View>();
  title_ = label_wrapper->AddChildView(
      std::make_unique<views::StyledLabel>(
          title, nullptr));

  if (!subtitle.empty()) {
    auto subtitle_label =
        std::make_unique<views::Label>(
            subtitle, views::style::CONTEXT_BUTTON,
            views::style::STYLE_SECONDARY);
    ...
    subtitle_ = label_wrapper->AddChildView(
        std::move(subtitle_label));
  }

  label_wrapper->SetLayoutManager(
      std::make_unique<views::FlexLayout>())
      ->SetOrientation(
          views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(
          views::LayoutAlignment::kCenter);
  label_wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::MinimumFlexSizeRule::kScaleToZero,
          views::MaximumFlexSizeRule::kUnbounded));
  label_wrapper->SetProperty(
      views::kMarginsKey,
      gfx::Insets(vertical_spacing, 0));
  label_wrapper_ =
      AddChildView(std::move(label_wrapper));
  ...

  if (secondary_view) {
    ...
    secondary_view->SetProperty(
        views::kMarginsKey,
        gfx::Insets(secondary_spacing,
                    icon_label_spacing,
                    secondary_spacing, 0));
    secondary_view_ =
        AddChildView(std::move(secondary_view));
  }
  ...
}
```

|||---|||

## Compute preferred/minimum sizes recursively from children

**Avoid hardcoding preferred or minimum sizes,** including via metrics like
[`DISTANCE_BUBBLE_PREFERRED_WIDTH`][].
In many cases, `LayoutManager`s will provide reasonable values for these,
and common codepaths like [`BubbleFrameView::GetFrameWidthForClientWidth()`][]
can help ensure that the returned values are [conformed to spec][].
When a `View` does need to calculate these manually, it should do so based on
the corresponding values returned by its children, not by returning specific
numbers (e.g. dialog preferred size is 300 by 150). In particular, assuming
fonts will be in a certain size, or that a given fixed area is sufficient to
display all necessary information, can cause hard-to-find localization and
accessibility bugs for users with verbose languages or unusually large fonts.

[`DISTANCE_BUBBLE_PREFERRED_WIDTH`]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/chrome_layout_provider.h;l=68;drc=ec62c9ac3ef71a7014e27c5d2cf98917a89e3524
[`BubbleFrameView::GetFrameWidthForClientWidth()`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/bubble/bubble_frame_view.cc;l=688;drc=f5df5da5753795298349b2dd6325e2c5e6e13e38
[conformed to spec]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/bubble/bubble_frame_view.cc;l=698;drc=f5df5da5753795298349b2dd6325e2c5e6e13e38

|||---|||

#####

**Avoid**

Current code overloads CalculatePreferredSize() in the dialog view.

#####

**Best practice**

A better approach would be to omit the overload completely and let leaf views
size the dialog appropriately, relying on the minimum size fallbacks
if necessary.

|||---|||

|||---|||

#####

``` cpp
...
gfx::Size
CastDialogView::CalculatePreferredSize() const {
  const int width =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}
```

#####

``` cpp
...







```

|||---|||


## Handle events directly, not via Layout()

In addition to using `LayoutManager`s in place of manual layout,
**avoid overriding `Layout()` to perform non-layout actions.** For example,
instead of updating properties tied to a `View`'s size in `Layout()`,
do so in [`OnBoundsChanged()`][];
when the `View` in question is a child, make the child a `View` subclass
with an `OnBoundsChanged()` override instead of having the parent both lay
the child out and update its properties. Modify the
[hit-testing and event-handling functions][] directly instead of laying out
invisible `View`s to intercept events. Toggle child visibility directly in
response to external events rather than calculating it inside `Layout()`.

[`OnBoundsChanged()`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h;l=1377;drc=34a8c4215229379ced3586125399c7ad3c65b87f
[hit-testing and event-handling functions]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h;l=919;drc=34a8c4215229379ced3586125399c7ad3c65b87f

|||---|||

#####

**Avoid**

Old code updated hit testing and button properties in the Layout() method.

#####

**Best practice**

Current code wraps the buttons in a file scoped class with an
OnBoundsChanged() method and modifies the hit testing functions directly to
achieve the same result.

|||---|||

|||---|||

#####

``` cpp











































FindBarView::FindBarView(FindBarHost* host)
    : find_bar_host_(host) {
  auto find_text = std::make_unique<views::Textfield>();
  find_text_ = AddChildView(std::move(find_text));
  ...
  auto find_previous_button =
      views::CreateVectorImageButton(this);
  find_previous_button_ =
      AddChildView(std::move(find_previous_button));
  ...
  auto find_next_button =
      views::CreateVectorImageButton(this);
  find_next_button_ =
      AddChildView(std::move(find_next_button));
  ...
  auto close_button =
      views::CreateVectorImageButton(this);
  close_button_ =
      AddChildView(std::move(close_button));
}

void FindBarView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  // The focus forwarder view is a hidden view that
  // should cover the area between the find text box
  // and the find button so that when the user clicks
  // in that area we focus on the find text box.
  const int find_text_edge =
      find_text_->x() + find_text_->width();
  focus_forwarder_view_->SetBounds(
      find_text_edge, find_previous_button_->y(),
      find_previous_button_->x() - find_text_edge,
      find_previous_button_->height());

  for (auto* button :
       {find_previous_button_, find_next_button_,
        close_button_}) {
    constexpr int kCircleDiameterDp = 24;
    auto highlight_path = std::make_unique<SkPath>();
    // Use a centered circular shape for inkdrops and
    // focus rings.
    gfx::Rect circle_rect(button->GetLocalBounds());
    circle_rect.ClampToCenteredSize(
        gfx::Size(kCircleDiameterDp,
                  kCircleDiameterDp));
    highlight_path->addOval(
        gfx::RectToSkRect(circle_rect));
    button->SetProperty(views::kHighlightPathKey,
                        highlight_path.release());
  }
}
```

#####

``` cpp
// An ImageButton that has a centered circular
// highlight.
class FindBarView::FindBarButton
    : public views::ImageButton {
 public:
  using ImageButton::ImageButton;
 protected:
  void OnBoundsChanged(
      const gfx::Rect& previous_bounds) override {
    const gfx::Rect bounds = GetLocalBounds();
    auto highlight_path = std::make_unique<SkPath>();
    const gfx::Point center = bounds.CenterPoint();
    const int radius = views::LayoutProvider::Get()
        ->GetCornerRadiusMetric(
            views::Emphasis::kMaximum, bounds.size());
    highlight_path->addCircle(
        center.x(), center.y(), radius);
    SetProperty(views::kHighlightPathKey,
                highlight_path.release());
  }
};

bool FindBarView::OnMousePressed(
    const ui::MouseEvent& event) {
  // The find text box only extends to the match count
  // label.  However, users expect to be able to click
  // anywhere inside what looks like the find text
  // box (including on or around the match_count label)
  // and have focus brought to the find box. Cause
  // clicks between the textfield and the find previous
  // button to focus the textfield.
  const int find_text_edge =
      find_text_->bounds().right();
  const gfx::Rect focus_area(
      find_text_edge, find_previous_button_->y(),
      find_previous_button_->x() - find_text_edge,
      find_previous_button_->height());
  if (!GetMirroredRect(focus_area).Contains(
      event.location()))
    return false;
  find_text_->RequestFocus();
  return true;

FindBarView::FindBarView(FindBarHost* host)
    : find_bar_host_(host) {
  auto find_text = std::make_unique<views::Textfield>();
  find_text_ = AddChildView(std::move(find_text));
  ...
  auto find_previous_button =
      std::make_unique<FindBarButton>(this);
  views::ConfigureVectorImageButton(
      find_previous_button.get());
  ...
  auto find_next_button =
      std::make_unique<FindBarButton>(this);
  views::ConfigureVectorImageButton(
      find_next_button.get());
  ...
  auto close_button =
      std::make_unique<FindBarButton>(this);
  views::ConfigureVectorImageButton(close_button.get());
}
































```

|||---|||

## Don't invoke DeprecatedLayoutImmediately()

**Avoid calls to `DeprecatedLayoutImmediately()`.**
These are typically used for three purposes:

1.  *Calling `DeprecatedLayoutImmediately()` on `this`, when something that
affects layout has changed.* This forces a synchronous layout, which can lead to
needless work (e.g. if several sequential changes each trigger layout). Use
asynchronous layout\* instead. In many cases (such as
[the preferred size changing][] or
[a child needing layout][],
a `View` will automatically mark itself as needing layout; when necessary, call
[`InvalidateLayout()`][] to mark it manually.

1.  *Calling `DeprecatedLayoutImmediately()` or `InvalidateLayout()` on some
`View` to notify it that something affecting its layout has changed.* Instead,
ensure that `View` is notified of the underlying change (via specific method
overrides or plumbing from a model object), and then invalidates its own layout
when needed.

1.  *Calling `DeprecatedLayoutImmediately()` on some `View` to "ensure it's up
to date" before reading some layout-related property off it.* Instead, plumb any
relevant events to the current object, then handle them directly (e.g. override
[`ChildPreferredSizeChanged()`][] or use a [`ViewObserver`][]
to monitor the target `View`; then update local state as necessary and trigger
handler methods).

\* *How does asynchronous layout work?* In the browser, the compositor
periodically [requests a LayerTreeHost update][].
This ultimately calls back to [`Widget::LayoutRootViewIfNecessary()`][],
recursively laying out invalidated `View`s within the `Widget`. In unittests,
this compositor-driven sequence never occurs, so it's necessary to
[call RunScheduledLayout() manually][] when a test needs to ensure a `View`'s
layout is up-to-date.  Many tests fail to do this, but currently pass because
something triggers Layout() directly; accordingly, changing existing code from
synchronous to asynchronous layout may require adding `RunScheduledLayout()`
calls to (possibly many) tests, and this is not a sign that the change is wrong.

[the preferred size changing]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.cc;l=1673;drc=bc9a6d40468646be476c61b6637b51729bec7b6d
[a child needing layout]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.cc;l=777;drc=bc9a6d40468646be476c61b6637b51729bec7b6d
[`InvalidateLayout()`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h;l=735;drc=c06f6b339b47ce2388624aa9a89334ace38a71e4
[`ChildPreferredSizeChanged()`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h;l=1381;drc=c06f6b339b47ce2388624aa9a89334ace38a71e4
[`ViewObserver`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view_observer.h;l=17;drc=eb20fd77330dc4a89eecf17459263e5895e7f177
[requests a LayerTreeHost update]: https://source.chromium.org/chromium/chromium/src/+/main:cc/trees/layer_tree_host.cc;l=304;drc=c06f6b339b47ce2388624aa9a89334ace38a71e4
[`Widget::LayoutRootViewIfNecessary()`]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/widget/widget.h;l=946;drc=b1dcb398c454a576092d38d0d67db3709b2b2a9b
[call RunScheduledLayout() manually]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/test/views_test_utils.h;l=17;drc=3e1a26c44c024d97dc9a4c09bbc6a2365398ca2c

|||---|||

#####

**Avoid**

[Current code][5] makes an unnecessary call to DeprecatedLayoutImmediately()

[5]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/media_router/cast_dialog_view.cc;l=283;drc=7db01ff4534c04419f3fa10d75e0b97b0a5a4f99

#####

**Best practice**

A better approach would be to call InvalidateLayout() and update the necessary tests.

|||---|||

|||---|||

#####

``` cpp
void CastDialogView::PopulateScrollView(
    const std::vector<UIMediaSink>& sinks) {
  ...
  DeprecatedLayoutImmediately();
}

TEST_F(CastDialogViewTest, PopulateDialog) {
  CastDialogModel model =
      CreateModelWithSinks({CreateAvailableSink()});
  InitializeDialogWithModel(model);

  EXPECT_TRUE(dialog_->ShouldShowCloseButton());
  EXPECT_EQ(model.dialog_header(),
            dialog_->GetWindowTitle());
  EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kNone),
            dialog_->GetDialogButtons());
}


```

#####

``` cpp
void CastDialogView::PopulateScrollView(
    const std::vector<UIMediaSink>& sinks) {
  ...
  InvalidateLayout();
}

TEST_F(CastDialogViewTest, PopulateDialog) {
  CastDialogModel model =
      CreateModelWithSinks({CreateAvailableSink()});
  InitializeDialogWithModel(model);
  CastDialogView::GetCurrentDialogWidget()
      ->LayoutRootViewIfNecessary();

  EXPECT_TRUE(dialog_->ShouldShowCloseButton());
  EXPECT_EQ(model.dialog_header(),
            dialog_->GetWindowTitle());
  EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kNone),
            dialog_->GetDialogButtons());
}
```

|||---|||


## Consider different objects for different layouts

If a surface needs very different appearances in different states (e.g. a
dialog whose content changes at each of several steps, or a container whose
layout toggles between disparate orientations), **use different `View`s to
contain the distinct states** instead of manually adding and removing
children and changing layout properties at each step. It's easier to reason
about several distinct fixed-layout `View`s than a single object whose layout
and children vary over time, and often more performant as well.

|||---|||

#####

**Avoid**

[Current code][6] holds both horizontal and vertical time views and replaces
the children and LayoutManager on orientation change.

[6]: https://source.chromium.org/chromium/chromium/src/+/main:ash/system/time/time_view.h;l=35;drc=7d8bc7f807a433e6a127806e991fe780aa27ce77;bpv=1;bpt=0?originalUrl=https:%2F%2Fcs.chromium.org%2F

#####

**Best practice**

A better approach would encapsulate the horizontal and vertical time views
into separate views.

|||---|||

|||---|||

#####

``` cpp
class ASH_EXPORT TimeView : public ActionableView,
                            public ClockObserver {
  ...
 private:
  ...
  std::unique_ptr<views::Label> horizontal_label_;
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



void TimeView::UpdateClockLayout(
    ClockLayout clock_layout) {
  // Do nothing if the layout hasn't changed.
  if (((clock_layout == ClockLayout::HORIZONTAL_CLOCK) ?
      horizontal_label_ : vertical_label_hours_)
      ->parent() == this)
    return;

  SetBorder(views::NullBorder());
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
    const int kColumnId = 0;
    views::ColumnSet* columns =
        layout->AddColumnSet(kColumnId);
    columns->AddPaddingColumn(
        0, kVerticalClockLeftPadding);
    columns->AddColumn(views::GridLayout::TRAILING,
                       views::GridLayout::CENTER,
                       0, views::GridLayout::USE_PREF,
                       0, 0);
    layout->AddPaddingRow(0, kClockLeadingPadding);
    layout->StartRow(0, kColumnId);
    // Add the views as existing since ownership isn't
    // being transferred.
    layout->AddExistingView(
        vertical_label_hours_.get());
    layout->StartRow(0, kColumnId);
    layout->AddExistingView(
        vertical_label_minutes_.get());
    layout->AddPaddingRow(
        0, kVerticalClockMinutesTopOffset);
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
  views::Label* time_label =
      AddChildView(std::make_unique<views::Label>());
  SetupLabels(time_label);
  ...
}

TimeView::VerticalLabelView::VerticalLabelView() {
  views::Label* label_hours =
      AddChildView(std::make_unique<views::Label>());
  views::Label* label_minutes =
      AddChildView(std::make_unique<views::Label>());
  SetupLabel(label_hours);
  SetupLabel(label_minutes);
  SetLayoutManager(
      std::make_unique<views::TableLayout>())
      ->AddPaddingColumn(
          views::TableLayout::kFixedSize,
          kVerticalClockLeftPadding)
      .AddColumn(
          views::LayoutAlignment::kEnd,
          views::LayoutAlignment::kCenter,
          views::TableLayout::kFixedSize,
          views::TableLayout::kUsePreferred, 0, 0)
      .AddPaddingRow(
          views::TableLayout::kFixedSize,
          kClockLeadingPadding)
      .AddRows(2, views::TableLayout::kFixedSize)
      .AddPaddingRow(
          views::TableLayout::kFixedSize,
          kVerticalClockMinutesTopOffset);
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
  InvalidateLayout();
}









































```

|||---|||

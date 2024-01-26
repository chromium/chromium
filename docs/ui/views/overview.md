# Views Overview

This document is an overview of Views concepts, terminology, and architecture.
The target audience is engineers using or working on Views.

## General Things

Points in this document are written as `(x,y)`, and rectangles are written as
`[(x,y) wxh]`. For example, the rectangle `[(100,100) 50x20]` contains the point
`(130,110)`.

Views uses a coordinate system with `(0,0)` at the top-left, with increasing
x-coordinates moving rightwards and increasing y-coordinates moving downwards.
This is the same as the Windows and GTK coordinate systems, but *different from*
the Cocoa coordinate system, which has `(0,0)` at the bottom-left. Coordinates
in this document use the Views coordinate system.

Views generally *take ownership* of objects passed to them even via raw
pointers, although there are some exceptions, such as delegates.

## Views

A **View** is a UI element, similar to an HTML DOM element. Each View occupies a
rectangle, called its **bounds**, which is expressed in the coordinate system of
its parent View. Views may have child Views, which are laid out according to the
View's **layout manager**, although individual Views may also override
`View::Layout` to implement their own layout logic. Each View may also have a
**border** and/or a **background**.

Each View can calculate different sizes, which are used by the View's parent
View to decide how to position and size it. Views may have any or all of a
preferred size, a minimum size, and a maximum size. These may instead be
calculated by the View's LayoutManager, and may be used by the parent View's
LayoutManager.

It is generally not a good idea to explicitly change the bounds of a View.
Typically, bounds are computed by the parent View's Layout method or the parent
View's LayoutManager. It is better to build a LayoutManager that does what you
want than to hand-layout Views by changing their bounds.

For more details about Views, see [view.h].

### Border

The **border** is conventionally drawn around the edges of the View's bounding
rectangle, and also defines the View's **content bounds**, which are the area
inside which the View's content is drawn. For example, a View that is at
`[(0,0) 100x100]` which has a solid border of thickness 2 will have content
bounds of `[(2,2) 96x96]`.

For more details, see [border.h].

### Background

The **background** is drawn below any other part of the View, including the
border. Any View can have a background, but most Views do not. A background is
usually responsible for filling the View's entire bounds. Backgrounds are
usually a color, but can be a gradient or something else entirely.

For more details, see [background.h].

### Content

The **content** is the area inside the content bounds of the View. A View's
child Views, if it has any, are also positioned and drawn inside the content
bounds of a View. There is no class representing the content area of a View; it
only exists as the space enclosed by the View's border, and its shape is defined
by the border's insets.

### Layout & Layout Managers

A View's **layout manager** defines how the View's child views should be laid
out within the View's content bounds. There are a few layout managers supplied
with Views. The simplest is [FillLayout], which lays out all a View's children
occupying the View's entire content bounds. [FlexLayout] provides a CSS-like
layout for horizontal and vertical arrangements of views.

Other commonly-used layouts managers are [BoxLayout], a predecessor of
FlexLayout, and [TableLayout], which provides a flexible row-and-column
system.

### Painting

Views are painted by pre-order traversal of the View tree - i.e., a parent View
is painted before its child Views are. Each View paints all its children in
z-order, as determined by `View::GetChildrenInZOrder()`, so the last child in
z-order is painted last and therefore over the previous children. The default
z-order is the order in which children were added to the parent View.

Different View subclasses implement their own painting logic inside
`View::OnPaint`, which by default simply calls `View::OnPaintBackground` and
`View::OnPaintBorder`. Generally, subclass implementations of `View::OnPaint`
begin by calling the superclass `View::OnPaint`.

If you need a special background or border for a View subclass, it is better to
create a subclass of `Background` or `Border` and install that, rather than
overriding `::OnPaintBackground` or `::OnPaintBorder`. Doing this helps preserve
the separation of Views into the three parts described above and makes painting
code easier to understand.

### Debugging

See [page](../ui_devtools/index.md) for details.

## Widgets

Views need an underlying canvas to paint onto. This has to be supplied by the
operating system, usually by creating a native drawing surface of some kind.
Views calls these **widgets**. A Widget is the bridge between a tree of Views
and a native window of some sort, which Views calls a **native widget**. Each
Widget has a **root view**, which is a special subclass of View that helps with
this bridging; the root view in turn has a single child view, called the
Widget's **contents view**, which fills the entire root view. All other Views
inside a given Widget are children of that Widget's contents view.

Widgets have many responsibilities, including but not limited to:

1. Keyboard focus management, via [FocusManager]
2. Window resizing/minimization/maximization
3. Window shaping, for non-rectangular windows
4. Input event routing

For more details, see [widget.h].

### Client and Non-Client Views

The contents view of most Widgets is a **Non-Client View**, which is either a
[NonClientView] or one of its descendants. The Non-Client View has two children,
which are the **Non-Client Frame View** (a [NonClientFrameView]) and the
**Client View** (a [ClientView]). The non-client frame view is responsible for
painting window decorations, the Widget's border, the shadow, and so on; the
client view is responsible for painting the Widget's contents. The area the
client view occupies is sometimes referred to as the Widget's "client area". The
non-client frame view may be swapped out as the system theme changes without
affecting the client view.

The overall structure of a Widget and its helper Views looks like this:

![views](images/views.png)

## Dialogs

A commonly-used type of client view is a **dialog client view**, which has a
**contents view**, optional buttons on the lower-right, and an optional extra
view on the lower-left. Dialogs are usually created by subclassing
[DialogDelegate] or [DialogDelegateView] and then calling
`DialogDelegate::CreateDialogWidget`. The dialog's contents view fills the
entire top part of the widget's client view, and the bottom part is taken over
by the dialog's buttons and extra view.

## Bubbles

A common type of dialog is a **bubble**, which is a dialog that is anchored to a
parent View and moves as the parent View moves. Bubbles are usually created by
subclassing [BubbleDialogDelegateView] and then calling
`BubbleDialogDelegateView::CreateBubble`. Bubbles can have a title, which is
drawn alongside the window controls as part of the Bubble's Widget's
NonClientFrameView.

[BoxLayout]: https://cs.chromium.org/chromium/src/ui/views/layout/box_layout.h
[BubbleDialogDelegateView]: https://cs.chromium.org/chromium/src/ui/views/bubble/bubble_dialog_delegate_view.h
[ClientView]: https://cs.chromium.org/chromium/src/ui/views/window/client_view.h
[DialogDelegate]: https://cs.chromium.org/chromium/src/ui/views/window/dialog_delegate.h
[DialogDelegateView]: https://cs.chromium.org/chromium/src/ui/views/window/dialog_delegate.h
[FillLayout]: https://cs.chromium.org/chromium/src/ui/views/layout/fill_layout.h
[FlexLayout]: https://cs.chromium.org/chromium/src/ui/views/layout/flex_layout.h
[FocusManager]: https://cs.chromium.org/chromium/src/ui/views/focus/focus_manager.h
[TableLayout]: https://cs.chromium.org/chromium/src/ui/views/layout/table_layout.h

[NonClientView]: https://cs.chromium.org/chromium/src/ui/views/window/non_client_view.h
[NonClientFrameView]: https://cs.chromium.org/chromium/src/ui/views/window/non_client_view.h
[background.h]: https://cs.chromium.org/chromium/src/ui/views/background.h
[border.h]: https://cs.chromium.org/chromium/src/ui/views/border.h
[canvas.h]: https://cs.chromium.org/chromium/src/ui/gfx/canvas.h
[view.h]: https://cs.chromium.org/chromium/src/ui/views/view.h
[widget.h]: https://cs.chromium.org/chromium/src/ui/views/widget/widget.h

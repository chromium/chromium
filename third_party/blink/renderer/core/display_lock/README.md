## Display Locking (a.k.a. content-visibility).

This directory is an implementation of a CSS feature
[content-visibility](https://drafts.csswg.org/css-contain/#content-visibility).

### Naming Conventions

Other than the places that rely on the name content-visibility as developer
exposed features, the code uses display-locking naming:

* Locked: this means that the context and the element itself is locked for
  display, and will not paint or hit-test its contents. The lock is acquired by
  content-visibility properties hidden and auto (when not on-screen).
* Unlocked: this means that the context and the element itself behave as if
  there was no display lock acquired, painting and laying out as usual.

### Brief structure overview

When a content-visibility property is specified with a value that may require
locking, a DisplayLockContext is created on the element's rare data. The state
of the context is updated by the style adjuster in response to style changes.
Additionally, the element's containment is also adjusted in style adjuster in
response to the actual state of the DisplayLockContext.

In lifecycle phases, such as style, layout, and paint, code checks
LayoutObject::ChildLayoutBlockedByDisplayLock and similar functions in order to
determine whether processing of work should happen. Note that the "self"
rendering always occurs, so the checks only exist for the "child" rendering.

Note that the LayoutObject checks delegate the decision to the
DisplayLockContext and can be thought of as helper functions.

The DisplayLockContext thus acts as an authoritative source of information when
it comes to deciding whether contents lifecycle should be processed in response
to content-visibility values. It is also responsible for blocking dirty-bit
propagation and restoring the dirty-bit state upon unlocking.

In addition to DisplayLockContext and LayoutObject helper functions, a
DisplayLockUtilities class is provided with a set of static functions that
perform common functionality, such as checking whether an element is within a
locked subtree without itself being locked.

### `hidden=until-found`

The `hidden=until-found` HTML attribute applies a style of
`content-visibility:hidden`, and also specially configures the DisplayLock to
unlock in response to find-in-page, scroll to text fragment navigation, and
element navigation.

### Ongoing work

This feature is new, and some work is continuing in the area.

We are working on developing an updateRendering javascript method
which allows asynchronous updates to locked / hidden display lock subtree.

# Tab Helpers

The `content/` layer of Chromium has a class called `WebContents`, which is one
of the most basic building blocks of all of Chromium. This document describes
how `WebContents`es are used to build tabs in browser windows.

Tab Helpers are deprecated for Desktop Chrome. Use TabFeatures instead. See
[design principles](chrome_browser_design_principles.md).

[TOC]

## Introduction

What is a "tab helper"? It is a `WebContentsObserver` owned by the `WebContents`
itself. Let's break that down.

## WebContentsObserver

`WebContentsObserver` is a
[simple interface](https://source.chromium.org/chromium/chromium/src/+/HEAD:content/public/browser/web_contents_observer.h)
that allows an object to observe events in the life of a `WebContents`. As an
example, if we look at the `TabStripModel`, there are times when it need to
watch out for WebContents being deleted. So it creates a
[TabStripModel::WebContentsData](https://source.chromium.org/chromium/chromium/src/+/HEAD:chrome/browser/ui/tabs/tab_strip_model.cc).
That object overrides `WebContentsDestroyed()`, and when a
`WebContents` gets destroyed, the callback is called and the object
processes the message. Note that `TabStripModel::WebContentsData` object is not owned by the
`WebContents`. It is owned indirectly by the `TabStripModel`.

## SupportsUserData and WebContentsUserData

There is a mechanism used in Chromium called
[`SupportsUserData`](https://source.chromium.org/chromium/chromium/src/+/HEAD:base/supports_user_data.h)
that allows attaching of arbitrary objects to an object. The mechanism is
simple: host objects derive from `SupportsUserData`, and owned objects derive
from `SupportsUserData::Data`. There are three calls to attach and detach the
data.

`WebContents` derives from `SupportsUserData`, so that mechanism works for
attaching objects to a `WebContents`, but the `SupportsUserData` mechanism is a
bit low-level. A higher level abstraction is
[`WebContentsUserData`](https://source.chromium.org/chromium/chromium/src/+/HEAD:content/public/browser/web_contents_user_data.h),
which is easy to derive from and has easy-to-use functionality in
`CreateForWebContents()` and `FromWebContents()`.

## Adding a feature to a browser tab

Let's combine `WebContentsObserver` and `WebContentsUserData` together, to log
whenever the title of a tab changes.

```
class TitleLoggerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TitleLoggerTabHelper> {
 public:
  TitleLoggerTabHelper(const TitleLoggerTabHelper&) = delete;
  TitleLoggerTabHelper& operator=(const TitleLoggerTabHelper&) = delete;
  ~TitleLoggerTabHelper() override;

  // content::WebContentsObserver
  void TitleWasSet(NavigationEntry* entry) override {
      LOG(INFO) << "Title: " << entry->GetTitle();
  }

 private:
  explicit TitleLoggerTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<TitleLoggerTabHelper>;
};
```

We want each tab to have this `WebContentsObserver` attached to it, so that it
will properly handle the events it's looking for, and when the tab goes away,
then this tab helper will go away too.

But how do you hook in to browser tab creation? How can we attach this tab
helper to the `WebContents`es that are used for the browser tabs?

## AttachTabHelpers

There is a function called
[`AttachTabHelpers()`](https://source.chromium.org/chromium/chromium/src/+/HEAD:chrome/browser/ui/tab_helpers.cc;).
Whenever a `WebContents` is created for use as a browser tab,
`AttachTabHelpers()` is called. Every tab helper from around Chromium,
from ContentSettings to Favicons to History to Prefs, all take this opportunity
to hook into those `WebContents` used as tabs.

If you are writing a feature that needs to deal with browser tabs, this is where
you go. Create a tab helper, and add it (in alphabetical order, please!) to
`AttachTabHelpers()`. Note, though, that you are _never_ allowed to call
`AttachTabHelpers()` yourself. `AttachTabHelpers()` is only for `WebContents`
that are in browser tabs, and all of those code paths are already written.

## Reusing tab helpers with non-browser tab WebContentses

Sometimes it's useful to re-use tab helpers for `WebContents`es that aren't
browser tabs. For example, the Chrome Apps code wants to be able to print, and
wants to use the printing code that browser tabs use. So in
[`ChromeAppDelegate::InitWebContents()`](https://source.chromium.org/chromium/chromium/src/+/HEAD:chrome/browser/ui/apps/chrome_app_delegate.cc)
we see that whenever the Apps code creates a new `WebContents`, it attaches a
carefully-chosen subset of tab helpers, including two printing ones.

You can do that too. If you are creating a `WebContents`, make a very deliberate
decision about which tab helpers you need. Chances are, you don't need them all;
you probably only need a handful. In fact, most tab helpers assume they are
attached to browser tabs, so only add the bare minimum.

## Not every WebContents has every tab helper

The other consequence of this design is that you can't make the assumption that
an arbitrary `WebContents` will have an arbitrary tab helper. The
`WebContents`es used as browser tabs likely will have most tab helpers (though
not necessarily all of them!) but a `WebContents` only has a tab helper if it is
installed on it.

The deeper (false and dangerous) assumption is that every `WebContents` is a
browser tab. Do not assume that either!

If your code handles `WebContents`es, be aware of their source. It is extremely
rare to have to be able to handle arbitrary `WebContents`es. Know where they
come from and what tab helpers are on them, and you'll be fine.

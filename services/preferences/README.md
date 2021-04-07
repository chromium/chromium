# Preference Service User Guide

The Preference Service is no longer an independent service. The implementation
was deleted: https://chromium-review.googlesource.com/c/chromium/src/+/1928248

Some legacy implementation is still available here to support current usage. It
has not been moved to be with the rest of the related preference code in
components/prefs.

## What are Preferences?

Preferences, also known as "prefs", are key-value pairs stored by
Chrome. Examples include the settings in chrome://settings, all per-extension
metadata, the list of plugins and so on. Individual prefs are keyed by a string
and have a type. E.g., the "browser.enable_spellchecking" pref stores a boolean
indicating whether spell-checking is enabled.

The pref service persists prefs to disk and communicates updates to prefs
between services, including Chrome itself. There is a pref service instance per
profile (prefs are persisted on a per-profile basis).

## Design

The original design doc is here:

https://docs.google.com/document/d/1JU8QUWxMEXWMqgkvFUumKSxr7Z-nfq0YvreSJTkMVmU/edit?usp=sharing

# Prepare for Code Review

* Use a //chrome/browser/ui/views/OWNERS owner when making substantial UI
  changes in order to ensure consistency with best practices **preferably via
  chromium-chrome-browser-ui-views-reviews@google.com** for changes in
  ``//chrome/browser/ui/views`. Hopefully your reviewer will know when to loop
  them in, but please do so proactively as well. This is good practice even if
  you have OWNERS approval from a subdirectory as that owner may be more
  familiar with the feature than UI best practices.
* If you're doing something that the framework doesn't seem to support, or is
  hard/complex, it probably means you shouldn't do it. Work with your UI dev
  contact at this point to resolve this (if you have one), or ask in the
  `#desktop-ui` Slack. This may require revisions to mocks if they are doing
  something that should not be supported.
* Hard-coded constants (colors, spacing values, etc.) are code smells; avoid at
  all costs. Become friends with
  [ThemeProvider](/ui/base/theme_provider.h) and
  [LayoutProvider](/ui/views/layout/layout_provider.h).
* Use a screen reader to navigate through the UI to make sure it's accessible.
  Make sure all items are correctly labeled / read out by the screenreader.
  *TODO([crbug.com/1238153](crbug.com/1238153)): Follow up with detailed
  instructions for getting started as part off a11y documentation and link those
  here.*
* Make sure your UI looks OK in OS dark mode and Incognito (including color
  contrast).
* Make sure your UI doesn't break if the strings are extra long (as they will be
  in some translations). Easiest way to do this is to modify the strings
  manually to double or triple their length.
* When you post UI code for review, include screenshots. The best way to do this
  is posting before (if relevant) and after screenshots as attachments on the
  linked bug. Mention this in the review.
  * When adding or changing strings, presubmit will prompt you to upload
    screenshots to facilitate translations. The associated hash can be used by
    reviewers to [view the screenshots](https://docs.google.com/document/d/1nwYWDny20icMSpLUuV_LgrlbWKrYpbXOERUIZNH636o/edit#heading=h.ndzwtb9u4qzw).

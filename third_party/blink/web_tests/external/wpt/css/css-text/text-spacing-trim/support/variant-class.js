'use strict';

/**
 * Add `class` search params to the `class` attribute of the root element.
 *
 * They are also added to `<link rel="match">` so that ref files have the same
 * `class` search parameters.
 */
if (window.location.search) {
  const params = new URLSearchParams(window.location.search);
  if (params.has('class')) {
    const classes = document.documentElement.classList;
    const links = document.querySelectorAll('link[rel="match"]');
    for (const value of params.getAll('class')) {
      classes.add(...value.split(','));

      for (const link of links) {
        const url = new URL(link.href);
        url.searchParams.append('class', value);
        link.href = url;
      }
    }
  }
}

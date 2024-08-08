
# Cumulative Layout Shift Changes in Chrome 97

### BFCache rollout causes big CLS improvements across the web

Chrome broadly rolled out the [BFCache](https://web.dev/bfcache) in January 2022.

## How does this affect a site's metrics?

For page loads which are restored from BFCache instead of being fully loaded on
a back/forward navigation, any layout shifts which occur during page load are
skipped. This can result in big improvements for sites with layout shifts during
page load.

## When were users affected?

The BFCache was widely rolled out in January 2022.
The CrUX report included this update starting with the January 2022 report.
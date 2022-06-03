This directory holds [services](/services) that are:
(a) Not considered part of Chrome's foundation (i.e., //services) or they have dependencies which we don't allow in /services (such as content or Blink), and
(b) are entirely consumed by content/ (or exposed via a content/public interface).

If in doubt about where your service belongs, contact services-dev@chromium.org.

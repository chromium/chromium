# Cumulative Layout Shift Changelog

This is a list of changes to [Cumulative Layout Shift](https://web.dev/cls).

* Chrome 89
  * Metric definition improvement: [Ignore layout shift when visibility:hidden becomes visible](2020_12_cls.md)
* Chrome 88
  * Metric definition improvement: [Cumulative layout shift properly detects shifts of fixed position elements](2020_11_cls.md)
  * Metric definition improvement: [Cumulative layout shift properly detects shifts of descendents of a sticky element](2020_11_cls.md)
  * Metric definition improvement: [no penalty for content-visibility: auto content](2020_11_cls.md)
* Chrome 87
  * Metric definition improvement: [Fix problem in Cumulative Layout shift calculation of impact region](2020_10_cls_2.md)
* Chrome 86
  * Metric definition changes and bug: [Cumulative Layout Shift score changes and regressions in impact region calculation](2020_10_cls.md)
* Chrome 85
  * Metric definition improvement: [Cumulative Layout Shift ignores layout shifts from video slider thumb](2020_06_cls.md)
* Chrome 79
  * Metric is elevated to stable; changes in metric definition will be reported in this log.
* Chrome 77
  * Metric exposed via API: [Cumulative Layout Shift](https://web.dev/cls/) available via [Layout Instability API](https://github.com/WICG/layout-instability)

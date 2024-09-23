# Cumulative Layout Shift Changelog

This is a list of changes to [Cumulative Layout Shift](https://web.dev/cls).

* Chrome 118
  * Implementation optimizations: [Image loading prioritizations](2023_10_image_loading_optimizations.md)
* Chrome 116
  * Implementation optimizations: [Optimizing image load scheduling](2023_08_image_loading.md)
* Chrome 98
  * Metric definition improvement: [Record CLS value at the first OnHidden in addition to tab close in UKM](2021_11_cls.md)
* Chrome 97
  * Implementation optimizations: [BFCache](2022_01_bfcache.md)
* Chrome 93
  * Metric definition improvement: [Bug fix involving scroll anchoring](2021_06_cls_2.md)
  * Metric definition improvement: [Ignore layout shift while dragging or resizing elements with a mouse](2021_06_cls_2.md)
* Chrome 91
  * Metric definition improvement: [Cumulative Layout Shift uses max session window](2021_06_cls.md)
* Chrome 90
  * Metric definition improvement: [Bug fixes involving changes to transform, effect, clip or position](2021_02_cls.md)
  * Metric definition improvement: [Consider transform change countering layout shift](2021_02_cls.md)
  * Metric definition improvement: [Ignore layout shift for more invisible elements](2021_02_cls.md)
  * Metric definition improvement: [Ignore inline direction shift moving from/to out of view](2021_02_cls.md)
  * Metric definition improvement: [Improvement for shift with counterscroll](2021_02_cls.md)
* Chrome 89
  * Metric definition improvement: [Ignore layout shift under opacity:0](2020_12_cls.md)
  * Metric definition improvement: [Clip layout shift rect by visual viewport](2020_12_cls.md)
* Chrome 88
  * Metric definition improvement: [Cumulative layout shift properly detects shifts of fixed position elements](2020_11_cls.md)
  * Metric definition improvement: [Cumulative layout shift properly detects shifts of descendents of a sticky element](2020_11_cls.md)
  * Metric definition improvement: [no penalty for content-visibility: auto content](2020_11_cls.md)
  * Metric definition improvement: [Ignore layout shift when visibility:hidden becomes visible](2020_11_cls.md)
* Chrome 87
  * Metric definition improvement: [Fix problem in Cumulative Layout shift calculation of impact region](2020_10_cls_2.md)
  * Metric definition improvement: [Cumulative Layout Shift properly handles clipping of elements styled contain:paint](2020_10_cls_2.md)
* Chrome 86
  * Metric definition changes and bug: [Cumulative Layout Shift score changes and regressions in impact region calculation](2020_10_cls.md)
* Chrome 85
  * Metric definition improvement: [Cumulative Layout Shift ignores layout shifts from video slider thumb](2020_06_cls.md)
* Chrome 79
  * Metric is elevated to stable; changes in metric definition will be reported in this log.
* Chrome 77
  * Metric exposed via API: [Cumulative Layout Shift](https://web.dev/cls/) available via [Layout Instability API](https://github.com/WICG/layout-instability)

## Version of ICU in chromium

This document documents which version of chromium has migrated to the major revision of public ICU release.

| ICU | Chrome version | Owner | Commit Date | Commit |
|-----|----------------|-------|-------------|--------|
| ICU-69 | 92.0.4484.0 | ftang | Apr 19, 2021 | [fe196e4a](https://chromiumdash.appspot.com/commit/fe196e4a3f221c17db92916f5291c75b2c5bd93a) |
| ICU-68 | 88.0.4316.0 | ftang | Nov 3, 2020 | [8669edbe](https://chromiumdash.appspot.com/commit/8669edbe8bbecb934bcccdb6c2ddfb9c57922157) |
| ICU-67 | 85.0.4162.0 | ftang | May 30, 2020 | [86dd70c3](https://chromiumdash.appspot.com/commit/86dd70c3717d487f9870dcda0e8372a98f2dccea) |
| ICU-66 | skipped |  |  |  |
| ICU-65 | 80.0.3956.0 | ftang | Oct 30, 2019 | [5679c3c1](https://chromiumdash.appspot.com/commit/5679c3c191ed62b62d8db22f1657a296ee9bfe8e) |
| ICU-64 | 75.0.3758.0 | ftang | Apr 4, 2019 | [56661356](https://chromiumdash.appspot.com/commit/566613568b38f2509a5f34aaf95dee7838cd4cca) |
| ICU-63 | 72.0.3596.0 | jshin | Oct 28, 2018 | [5afd300](https://chromiumdash.appspot.com/commit/5afd3007276ae994a1c9fc5bb7da7363babb76e3) |
| ICU-62 | 69.0.3488.0 | jshin | Jul 10, 2018 | [e9126f1](https://chromiumdash.appspot.com/commit/e9126f1d03725c2ae97d524985971d66089eede3) |
| ICU-61 | 68.0.3425.0 | jshin | May 8, 2018 | [359c346](https://chromiumdash.appspot.com/commit/359c346836e261c50fb4d3168f5e0a3ce4edb436) |
| ICU-60 | 64.0.3263.0 | jshin | Nov 7, 2017 | [befb166](https://chromiumdash.appspot.com/commit/befb16634bb440cf5442979ad262832b4cebd43e) |
| prior to ICU-60 | | | |  |

#### Note:

* Regular major release schedule for ICU is about 6 months - around each April/May and Oct/Nov.
* Out of cycle special releases of ICU happen from time to time, for example ICU-62 and ICU-66.
* See [Downloading ICU page](http://site.icu-project.org/download) for the
information on the CLDR version of a particular ICU version.
* We start to update the copy of ICU inside chromium around the time ICU
published the ICU release candidate and usually complete the migration about 2-4
weeks after the public ICU release.
* Patches of ICU bug fix for a later release might be cherry picked before the
official ICU release, depending on priority and severity of the defect.

#### Document Source:
 * [Chrome dashboard for ftang@ commits](https://chromiumdash.appspot.com/commits?user=ftang)
 * [Chrome dashboard for jshin@ commits](https://chromiumdash.appspot.com/commits?user=jshin)

# Stable Test Exclusion filters

These filter files are generated from the failure history of tests in CQ.
Currently the criteria for being added to these lists is for the test to have
not failed in the past 6 months. There is no auto roller for these filter files
and they are only being applied to experimental builders for assessment with
quickrun at this time. The files are organized by builder name so builders with
stable test exclusion enabled know where to find the applicable filter file.
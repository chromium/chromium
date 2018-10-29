importScripts('/resources/testharness.js',
              '/resources/origin-trials-helper.js');

test(t => {
  OriginTrialsHelper.check_interfaces_missing(
    self,
    ['BackgroundFetchEvent', 'BackgroundFetchFetch', 'BackgroundFetchManager',
     'BackgroundFetchUpdateUIEvent', 'BackgroundFetchRecord',
     'BackgroundFetchRegistration']);
}, 'Background Fetch API interfaces in Origin-Trial disabled worker.');

done();

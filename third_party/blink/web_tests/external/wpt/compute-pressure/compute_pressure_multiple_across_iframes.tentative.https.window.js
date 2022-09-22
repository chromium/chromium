'use strict';

promise_test(async t => {
  const update1_promise = new Promise((resolve, reject) => {
    const observer = new PressureObserver(resolve, {sampleRate: 1.0});
    t.add_cleanup(() => observer.disconnect());
    observer.observe('cpu').catch(reject);
  });

  // iframe numbers are aligned with observer numbers. The first observer is
  // in the main frame, so there is no iframe1.
  const iframe2 = document.createElement('iframe');
  document.body.appendChild(iframe2);

  const update2_promise = new Promise((resolve, reject) => {
    const observer =
        new iframe2.contentWindow.PressureObserver(resolve, {sampleRate: 1.0});
    t.add_cleanup(() => observer.disconnect());
    observer.observe('cpu').catch(reject);
  });

  const iframe3 = document.createElement('iframe');
  document.body.appendChild(iframe3);

  const update3_promise = new Promise((resolve, reject) => {
    const observer =
        new iframe3.contentWindow.PressureObserver(resolve, {sampleRate: 1.0});
    t.add_cleanup(() => observer.disconnect());
    observer.observe('cpu').catch(reject);
  });

  const [update1, update2, update3] =
      await Promise.all([update1_promise, update2_promise, update3_promise]);

  for (const update of [update1, update2, update3]) {
    assert_in_array(
        update.state, ['nominal', 'fair', 'serious', 'critical'],
        'cpu pressure state');
  }
}, 'Three PressureObserver instances, in different iframes, receive updates');

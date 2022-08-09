function runTest({target, eventName, passive, expectCancelable}) {
  let cancelable = null;
  let arrived = false;
  target.addEventListener(eventName, function (event) {
    cancelable = event.cancelable;
    arrived = true;
  }, {passive:passive, once:true});

  promise_test(async (t) => {
    t.add_cleanup(() => {
      document.querySelector('.remove-on-cleanup')?.remove();
    });
    const pos_x = Math.floor(window.innerWidth / 2);
    const pos_y = Math.floor(window.innerHeight / 2);
    const delta_x = 0;
    const delta_y = 100;
    await new test_driver.Actions()
      .scroll(pos_x, pos_y, delta_x, delta_y).send();
    await waitFor(() => arrived);
    assert_equals(cancelable, expectCancelable);
  });
}

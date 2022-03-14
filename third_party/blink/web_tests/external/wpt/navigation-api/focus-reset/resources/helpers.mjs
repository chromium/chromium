// Usage note: if you use these more than once in a given file, be sure to
// clean up any navigate event listeners, e.g. by using { once: true }, between
// tests.

export function testFocusWasReset(setupFunc, description) {
  promise_test(async t => {
    setupFunc(t);

    const button = document.body.appendChild(document.createElement("button"));
    t.add_cleanup(() => { button.remove(); });

    assert_equals(document.activeElement, document.body, "Start on body");
    button.focus();
    assert_equals(document.activeElement, button, "focus() worked");

    const { committed, finished } = navigation.navigate("#" + location.hash.substring(1) + "1");

    await committed;
    assert_equals(document.activeElement, button, "Focus stays on the button during the transition");

    await finished.catch(() => {});
    assert_equals(document.activeElement, document.body, "Focus reset after the transition");
  }, description);
}

export function testFocusWasNotReset(setupFunc, description) {
  promise_test(async t => {
    setupFunc(t);

    const button = document.body.appendChild(document.createElement("button"));
    t.add_cleanup(() => { button.remove(); });

    assert_equals(document.activeElement, document.body, "Start on body");
    button.focus();
    assert_equals(document.activeElement, button, "focus() worked");

    const { committed, finished } = navigation.navigate("#" + location.hash.substring(1) + "1");

    await committed;
    assert_equals(document.activeElement, button, "Focus stays on the button during the transition");

    await finished.catch(() => {});
    assert_equals(document.activeElement, button, "Focus stays on the button after the transition");
  }, description);
}

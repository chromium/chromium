// TODO(domenic): consider using these in all test files.

window.createRecordingCloseWatcher = (t, events, name) => {
  const prefix = name === undefined ? "" : name + " ";;

  const watcher = new CloseWatcher();
  t.add_cleanup(() => watcher.destroy());
  watcher.oncancel = () => events.push(prefix + "cancel");
  watcher.onclose = () => events.push(prefix + "close");

  return watcher;
};

window.createBlessedRecordingCloseWatcher = (t, events, name) => {
  return test_driver.bless("create " + name, () => createRecordingCloseWatcher(t, events, name));
};

window.sendCloseSignal = () => {
  // *not* \uu001B; see https://w3c.github.io/webdriver/#keyboard-actions
  const ESC = '\uE00C';

  return test_driver.send_keys(document.getElementById("d"), ESC);
};

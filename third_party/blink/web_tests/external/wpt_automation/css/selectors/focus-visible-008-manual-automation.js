importAutomationScript('/input-events/inputevent_common_input.js');

function inject_input() {
  return keyDown("Tab").then(() => {
      return keyDown("Enter");
  });
};

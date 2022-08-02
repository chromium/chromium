async function wait_for_toggle_creation(element) {
  // The spec is vague about when toggles need to be created.  Flushing
  // style is good enough for now, but this might need to change.
  getComputedStyle(element).toggleRoot;
}

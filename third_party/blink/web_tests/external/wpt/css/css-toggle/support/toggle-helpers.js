async function wait_for_toggle_creation(element) {
  // The spec is vague about when toggles need to be created.  Flushing
  // style is good enough for now, but this might need to change.
  getComputedStyle(element).toggleRoot;
}

async function set_up_single_toggle_in(container, toggle_style) {
  let div = document.createElement("div");
  div.style.toggle = toggle_style;
  container.replaceChildren(div);
  await wait_for_toggle_creation(div);
  return div;
}

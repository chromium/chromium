/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   Supplemental JS for disclosure card event handling
 */

'use strict';

document.addEventListener('click', (e) => {
  handleInteraction(e, true);
});
document.addEventListener('mousedown', (e) => {
  handleInteraction(e, false);
});
document.addEventListener('mouseup', handlePressEnd);

function handleInteraction(e, isClick) {
  const disclosureCard = e.target.closest('.disclosure-card');
  if (!disclosureCard) return;

  const button = disclosureCard.querySelector('button[aria-expanded]');
  const isTextSelected = window.getSelection().toString().length > 0;
  const target = e.target;
  const targetTag = target.tagName.toLowerCase();

  // Don’t proceed if the user has:
  // - Triggered event on any nested focusable other than the card’s button
  // - Triggered event on a label element
  // - Triggered event via text selection
  if (
    (button !== target && (isFocusable(target) || targetTag === 'label')) ||
    isTextSelected
  )
    return;

  if (isClick) {
    const isExpanded = button.ariaExpanded === 'true';
    const details = disclosureCard.querySelector('.details');
    button.ariaExpanded = isExpanded ? 'false' : 'true';
    details.inert = isExpanded;
    button.focus();
  } else {
    disclosureCard.setAttribute('data-being-pressed', 'true');
  }
}

function handlePressEnd() {
  document
    .querySelector('.disclosure-card[data-being-pressed]')
    ?.removeAttribute('data-being-pressed');
}

const isFocusable = (element) => element.tabIndex >= 0 && !element.disabled;

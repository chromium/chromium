function logKeyboardMovementResult(logContainer, keyboardInputType, colorSelectionRingOwnerName, colorSelectionRing,
    colorSelectionRingOriginalLeft, colorSelectionRingOriginalTop) {
  if(isColorSelectionRingAtSamePosition(colorSelectionRing,
      colorSelectionRingOriginalLeft, colorSelectionRingOriginalTop)) {
    logContainer.append(keyboardInputType + ' did not move ' + colorSelectionRingOwnerName + ' selection ring.');
  } else {
    logContainer.append(keyboardInputType + ' did move ' + colorSelectionRingOwnerName + ' selection ring.');
  }
  logContainer.append(document.createElement('br'));
}

function isColorSelectionRingAtSamePosition(colorSelectionRing, colorSelectionRingOriginalLeft, colorSelectionRingOriginalTop) {
  return (colorSelectionRingOriginalLeft === colorSelectionRing.left) && (colorSelectionRingOriginalTop === colorSelectionRing.top);
}

new MutationObserver(mutations => {
  console.log('from observer');
  debugger;
}).observe(document.body, {
  childList: true,
  subtree: true
});

setTimeout(() => {
  document.body.appendChild(document.createElement('div'));
}, 0);

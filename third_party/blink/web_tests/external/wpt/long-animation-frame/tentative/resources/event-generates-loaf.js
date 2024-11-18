(() => {
  const xhr = new XMLHttpRequest();
  xhr.open('GET', '/common/dummy.xml');
  xhr.addEventListener('load', () => {
    window.busy_wait();
  });
  xhr.send();
})();

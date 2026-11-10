export function createLink(t, specifier, href) {
  const link = document.createElement('link');
  t.add_cleanup(() => link.remove());
  link.rel = 'stylesheet';
  link.setAttribute('import', specifier);
  if (href !== undefined) {
    link.href = href;
  }
  return link;
}

export function loadResult(t, specifier, href) {
  return appendAndWait(createLink(t, specifier, href));
}

export function appendAndWait(link) {
  return new Promise((resolve) => {
    link.addEventListener('load', () => resolve({link, event: 'load'}),
                          {once: true});
    link.addEventListener('error', () => resolve({link, event: 'error'}),
                          {once: true});
    document.head.appendChild(link);
  });
}

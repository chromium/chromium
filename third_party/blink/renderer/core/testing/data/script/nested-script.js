const script = document.createElement('script');
script.type = 'text/javascript';
script.textContent = `
const start = Date.now();
while (Date.now() - start <= 200) {
  // Simulate a long-running script
}
`;
document.body.appendChild(script);

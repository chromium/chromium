{
  const id = '{{GET[id]}}';
  if (id === '' || id === String(window.current_test_id)) {
    window.events.push('slow');
    window.on_slow?.();
  }
}

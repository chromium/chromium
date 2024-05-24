#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_RECORD_REPLAY_LIFECYCLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_RECORD_REPLAY_LIFECYCLE_H_

namespace blink {

class Page;

} // namespace blink

namespace recordreplay {

void NotifyPageVisibilityStateChanged(blink::Page* page);
void NotifyPageFocusControllerActiveChanged(blink::Page* page);

void NotifyPageWillBeDestroyed(blink::Page* page);

} // namespace recordreplay

#endif // THIRD_PARTY_BLINK_RENDERER_CORE_RECORD_REPLAY_LIFECYCLE_H_

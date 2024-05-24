
#include "base/record_replay_paint_surface.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/record_replay/lifecycle.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

#define LOG_LIFECYCLE_EVENTS 1


namespace {
// the current set of ordinary pages, presumably scoped to this renderer process
// (TODO verify this last part) lives at blink::Page::OrdinaryPages().  We
// should be able to use this to get the JS contexts for each main frame, but I
// haven't traced through.

// There might be a reference to our last-active page someplace in blink
// already, but until/unless we find it, we'll update it here.  It should be
// safe to keep the raw pointer to the page, since we'll clear it when the page
// is destroyed.
static blink::Page* gLastActivePage = nullptr;

static void SetLastActivePage(blink::Page* page) {
    if (page == gLastActivePage) {
        // it's already the active page
        return;
    }

#if LOG_LIFECYCLE_EVENTS
    recordreplay::Print("[ContextRoots] updating last-active page");
#endif
    gLastActivePage = page;

    // signal that the paint surface is about to change, so we don't ignore
    // the paints to the surface corresponding to our now active page.

    // TODO(toshok): we need recordreplay::DoSetPaintSurface (which doesn't yet
    // exist) here instead of recordreplay::DoResetPaintSurface.
    //
    // Resetting the paint surface will cause us to choose the surface we paint
    // next as the active paint surface. This won't always be what we want, I
    // think - we want the surface corresponding to the page we're setting as
    // the active page.
    //
    // Imagine multiple windows, both showing a page from the same renderer
    // process.  If page 1 holds the active paint surface and we click on page
    // 2, we want to move the active surface over to page 2.  But if page 1 has
    // an async paint queued up, it might paint before page 2 does, and remain
    // the active surface.
    //
    // While I think this is fine for now - resetting the paint surface more
    // often will cause us to be correct more often - it won't land us 100%
    // where we want to be.
    recordreplay::DoResetPaintSurface();
}

static void ResetLastActivePage() {
#if LOG_LIFECYCLE_EVENTS
    recordreplay::Print("[ContextRoots] clearing last-active page");
#endif
    gLastActivePage = nullptr;
}

#if LOG_LIFECYCLE_EVENTS
static std::string PageDescription(blink::Page* page) {
    std::string main_frame_info;

    if (auto* main_local_frame = blink::DynamicTo<blink::LocalFrame>(page->MainFrame())) {
        main_frame_info = main_local_frame->GetDocument()->Url().GetString().Utf8();
    } else {
        main_frame_info = "<unknown>";
    }

    std::stringstream desc;
    desc << "Page(" << main_frame_info << ")";
    return desc.str();
}
#endif

} // namespace

namespace recordreplay {

void NotifyPageVisibilityStateChanged(blink::Page* page) {
    if (!recordreplay::IsRecordingOrReplaying()) {
        return;
    }

#if LOG_LIFECYCLE_EVENTS
    std::stringstream to;
    to << page->GetVisibilityState();
    recordreplay::Print("[ContextRoots] %s visibility changed to %s", PageDescription(page).c_str(), to.str().c_str());
#endif

    // When a page is focused and is made visible, it becomes our last-active page.
    if (page->GetFocusController().IsActive() &&
        page->GetVisibilityState() == blink::mojom::blink::PageVisibilityState::kVisible) {

        SetLastActivePage(page);
    }
}

void NotifyPageFocusControllerActiveChanged(blink::Page* page) {
    if (!recordreplay::IsRecordingOrReplaying()) {
        return;
    }

#if LOG_LIFECYCLE_EVENTS
    recordreplay::Print("[ContextRoots] %s focus controller changed to %s", PageDescription(page).c_str(), page->GetFocusController().IsActive() ? "active" : "inactive");
#endif

    // When a page visible and then focused, it becomes our last-active page.
    if (page->GetFocusController().IsActive() &&
        page->GetVisibilityState() == blink::mojom::blink::PageVisibilityState::kVisible) {

        SetLastActivePage(page);
    }

}

void NotifyPageWillBeDestroyed(blink::Page* page) {
    if (!recordreplay::IsRecordingOrReplaying()) {
        return;
    }

#if LOG_LIFECYCLE_EVENTS
    recordreplay::Print("[ContextRoots] %s will be destroyed", PageDescription(page).c_str());
#endif

    if (page == gLastActivePage) {
        ResetLastActivePage();
    }
}

} // namespace recordreplay

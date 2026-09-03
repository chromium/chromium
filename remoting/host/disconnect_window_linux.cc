// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include <algorithm>
#include <memory>
#include <numbers>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/i18n/char_iterator.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_window.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

namespace {

// The amount of time to wait before allowing another position toggle.
constexpr base::TimeDelta kToggleCooldown = base::Seconds(3);

// Maximum length of the username / client identity in UTF-16 characters.
constexpr size_t kMaxUsernameLength = 50;

// Margins from screen edges to ensure the dialog is not obscured by the top bar
// or an auto-hiding dock/panel at the bottom.
constexpr int kTopMargin = 40;
constexpr int kBottomMargin = 60;

enum class WindowAnchor {
  kBottom,
  kTop,
};

// Remembers the last selected anchor position across dialog instances.
WindowAnchor g_current_anchor = WindowAnchor::kBottom;

class DisconnectWindowGtk : public HostWindow {
 public:
  DisconnectWindowGtk();

  DisconnectWindowGtk(const DisconnectWindowGtk&) = delete;
  DisconnectWindowGtk& operator=(const DisconnectWindowGtk&) = delete;

  ~DisconnectWindowGtk() override;

  // HostWindow overrides.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override;

 private:
  gboolean OnDelete(GtkWidget* window, GdkEvent* event);
  void OnClicked(GtkButton* button);
  void OnToggleClicked(GtkButton* button);
  gboolean OnConfigure(GtkWidget* widget, GdkEventConfigure* event);
  gboolean OnDraw(GtkWidget* widget, cairo_t* cr);
#if !GTK_CHECK_VERSION(3, 90, 0)
  void OnMonitorsChanged(GdkScreen* screen);
#endif

  // Positions the dialog window based on the current anchor.
  void SetDialogPosition();

  // Toggles the dialog anchor between top and bottom.
  void ToggleAlignment();

  // Re-enables the toggle button when cooldown expires.
  void OnCooldownExpired();

  // Updates the toggle button text according to the current anchor.
  void UpdateToggleButtonText();

  // Used to disconnect the client session.
  base::WeakPtr<ClientSessionControl> client_session_control_;

  base::OneShotTimer cooldown_timer_;

  raw_ptr<GtkWidget> disconnect_window_;
  raw_ptr<GtkWidget> toggle_button_;
  raw_ptr<GtkWidget> message_;
  raw_ptr<GtkWidget> button_;

  // Used to distinguish resize events from other types of "configure-event"
  // notifications.
  int current_width_;
  int current_height_;

  std::vector<ScopedGSignal> signals_;

  base::WeakPtrFactory<DisconnectWindowGtk> weak_factory_{this};
};

// Helper function for creating a rectangular path with rounded corners, as
// Cairo doesn't have this facility.  |radius| is the arc-radius of each
// corner.  The bounding rectangle extends from (0, 0) to (width, height).
void AddRoundRectPath(cairo_t* cairo_context,
                      int width,
                      int height,
                      int radius) {
  cairo_new_sub_path(cairo_context);
  cairo_arc(cairo_context, width - radius, radius, radius,
            -std::numbers::pi / 2, 0);
  cairo_arc(cairo_context, width - radius, height - radius, radius, 0,
            std::numbers::pi / 2);
  cairo_arc(cairo_context, radius, height - radius, radius,
            std::numbers::pi / 2, std::numbers::pi);
  cairo_arc(cairo_context, radius, radius, radius, std::numbers::pi,
            3 * std::numbers::pi / 2);
  cairo_close_path(cairo_context);
}

// Renders the disconnect window background.
void DrawBackground(cairo_t* cairo_context, int width, int height) {
  // Set the arc radius for the corners.
  const int kCornerRadius = 6;

  // Initialize the whole bitmap to be transparent.
  cairo_save(cairo_context);
  cairo_set_source_rgba(cairo_context, 0, 0, 0, 0);
  cairo_set_operator(cairo_context, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cairo_context);
  cairo_restore(cairo_context);

  AddRoundRectPath(cairo_context, width, height, kCornerRadius);
  cairo_clip(cairo_context);

  // Paint the whole bitmap one color.
  cairo_set_source_rgb(cairo_context, 0.91, 0.91, 0.91);
  cairo_paint(cairo_context);

  // Paint the round-rectangle edge.
  cairo_set_source_rgb(cairo_context, 0.13, 0.69, 0.11);
  cairo_set_line_width(cairo_context, 6);
  AddRoundRectPath(cairo_context, width, height, kCornerRadius);
  cairo_stroke(cairo_context);
}

DisconnectWindowGtk::DisconnectWindowGtk()
    : disconnect_window_(nullptr),
      toggle_button_(nullptr),
      message_(nullptr),
      button_(nullptr),
      current_width_(0),
      current_height_(0) {}

DisconnectWindowGtk::~DisconnectWindowGtk() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disconnect_window_) {
    signals_.clear();
    cooldown_timer_.Stop();
    toggle_button_ = nullptr;
    message_ = nullptr;
    button_ = nullptr;
    gtk_widget_destroy(disconnect_window_.ExtractAsDangling());
  }
}

void DisconnectWindowGtk::Start(
    const base::WeakPtr<ClientSessionControl>& client_session_control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!client_session_control_.get());
  DCHECK(client_session_control.get());
  DCHECK(!disconnect_window_);

  client_session_control_ = client_session_control;

  // Create the window.
  disconnect_window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  GtkWindow* window = GTK_WINDOW(disconnect_window_.get());

  auto connect = [&](auto* sender, const char* detailed_signal, auto receiver) {
    // Unretained() is safe since DisconnectWindowGtk will own the
    // ScopedGSignal.
    signals_.emplace_back(
        sender, detailed_signal,
        base::BindRepeating(receiver, base::Unretained(this)));
  };

  connect(disconnect_window_.get(), "delete-event",
          &DisconnectWindowGtk::OnDelete);
  gtk_window_set_title(window,
                       l10n_util::GetStringUTF8(IDS_PRODUCT_NAME).c_str());
  gtk_window_set_resizable(window, FALSE);

  // Try to keep the window always visible.
  gtk_window_stick(window);
  gtk_window_set_keep_above(window, TRUE);

  // Remove window titlebar.
  gtk_window_set_decorated(window, FALSE);

  // In case the titlebar is still there, try to remove some of the buttons.
  // Utility windows have no minimize button or taskbar presence.
  gtk_window_set_type_hint(window, GDK_WINDOW_TYPE_HINT_UTILITY);
  gtk_window_set_deletable(window, FALSE);

  // Allow custom rendering of the background pixmap.
#if !GTK_CHECK_VERSION(3, 90, 0)
  gtk_widget_set_app_paintable(disconnect_window_.get(), TRUE);
#endif
  connect(disconnect_window_.get(), "draw", &DisconnectWindowGtk::OnDraw);

  // Handle window resizing, to regenerate the background pixmap and window
  // shape bitmap.  The stored width & height need to be initialized here
  // in case the window is created a second time (the size of the previous
  // window would be remembered, preventing the generation of bitmaps for the
  // new window).
  current_height_ = current_width_ = 0;
  connect(disconnect_window_.get(), "configure-event",
          &DisconnectWindowGtk::OnConfigure);

  // Layout contains: toggle button, message label, and disconnect button.
  GtkWidget* button_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_set_homogeneous(GTK_BOX(button_row), FALSE);

#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_widget_set_margin_start(GTK_WIDGET(button_row), 12);
  gtk_widget_set_margin_end(GTK_WIDGET(button_row), 12);
  gtk_widget_set_margin_top(GTK_WIDGET(button_row), 8);
  gtk_widget_set_margin_bottom(GTK_WIDGET(button_row), 8);
  gtk_container_add(GTK_CONTAINER(window), button_row);
#else
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  GtkWidget* align = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(align), 8, 8, 12, 12);
  G_GNUC_END_IGNORE_DEPRECATIONS;
  gtk_container_add(GTK_CONTAINER(window), align);
  gtk_container_add(GTK_CONTAINER(align), button_row);
#endif

  toggle_button_ = gtk_button_new();
  UpdateToggleButtonText();
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_box_pack_start(GTK_BOX(button_row), toggle_button_.get());
#else
  gtk_box_pack_start(GTK_BOX(button_row), toggle_button_.get(), FALSE, FALSE,
                     0);
#endif
  connect(GTK_BUTTON(toggle_button_.get()), "clicked",
          &DisconnectWindowGtk::OnToggleClicked);

  message_ = gtk_label_new(nullptr);
  gtk_label_set_ellipsize(GTK_LABEL(message_.get()), PANGO_ELLIPSIZE_MIDDLE);
  gtk_label_set_max_width_chars(GTK_LABEL(message_.get()), 30);
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_box_pack_start(GTK_BOX(button_row), message_.get());
  gtk_widget_set_hexpand(message_.get(), TRUE);
#else
  gtk_box_pack_start(GTK_BOX(button_row), message_.get(), TRUE, TRUE, 0);
#endif

  button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_STOP_SHARING_BUTTON).c_str());
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_box_pack_end(GTK_BOX(button_row), button_.get());
#else
  gtk_box_pack_end(GTK_BOX(button_row), button_.get(), FALSE, FALSE, 0);
#endif

  connect(GTK_BUTTON(button_.get()), "clicked",
          &DisconnectWindowGtk::OnClicked);

  // Override any theme setting for the text color, so that the text is
  // readable against the window's background pixmap.
  PangoAttrList* attributes = pango_attr_list_new();
  PangoAttribute* text_color = pango_attr_foreground_new(0, 0, 0);
  pango_attr_list_insert(attributes, text_color);
  gtk_label_set_attributes(GTK_LABEL(message_.get()), attributes);
  pango_attr_list_unref(attributes);

#if !GTK_CHECK_VERSION(3, 90, 0)
  // GTK4 always uses an RGBA visual for windows.
  GdkScreen* screen = gtk_widget_get_screen(disconnect_window_.get());
  if (screen) {
    connect(screen, "monitors-changed",
            &DisconnectWindowGtk::OnMonitorsChanged);
    connect(screen, "size-changed", &DisconnectWindowGtk::OnMonitorsChanged);
    GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
    if (visual) {
      gtk_widget_set_visual(disconnect_window_.get(), visual);
    }
  }

  // GTK4 shows windows by default.
  gtk_widget_show_all(disconnect_window_.get());
#endif

  // Extract the user name from the JID.
  std::string client_jid = client_session_control_->client_jid();
  std::u16string username =
      base::UTF8ToUTF16(client_jid.substr(0, client_jid.find('/')));
  username = base::CollapseWhitespace(username,
                                      /*trim_sequences_with_line_breaks=*/true);
  // Truncate username safely at a Unicode character boundary so that
  // truncation does not split UTF-16 surrogate pairs. Truncating before
  // formatting ensures localized punctuation and grammar in IDS_MESSAGE_SHARED
  // (e.g. trailing periods) are preserved.
  if (username.length() > kMaxUsernameLength) {
    username.erase(
        base::i18n::UTF16CharIterator::LowerBound(username, kMaxUsernameLength)
            .array_pos());
  }

  std::string message_text =
      l10n_util::GetStringFUTF8(IDS_MESSAGE_SHARED, username);
  gtk_label_set_text(GTK_LABEL(message_.get()), message_text.c_str());
  SetDialogPosition();
  gtk_window_present(window);
}

void DisconnectWindowGtk::OnClicked(GtkButton* button) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (client_session_control_.get()) {
    client_session_control_->DisconnectSession(
        ErrorCode::OK, "Disconnect button was clicked.", FROM_HERE);
  }
}

void DisconnectWindowGtk::OnToggleClicked(GtkButton* button) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ToggleAlignment();
}

void DisconnectWindowGtk::ToggleAlignment() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  g_current_anchor = (g_current_anchor == WindowAnchor::kBottom)
                         ? WindowAnchor::kTop
                         : WindowAnchor::kBottom;
  UpdateToggleButtonText();
  if (toggle_button_) {
    gtk_widget_set_sensitive(toggle_button_.get(), FALSE);
    cooldown_timer_.Start(
        FROM_HERE, kToggleCooldown,
        base::BindOnce(&DisconnectWindowGtk::OnCooldownExpired,
                       weak_factory_.GetWeakPtr()));
  }
  SetDialogPosition();
}

void DisconnectWindowGtk::OnCooldownExpired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (toggle_button_) {
    gtk_widget_set_sensitive(toggle_button_.get(), TRUE);
  }
}

void DisconnectWindowGtk::UpdateToggleButtonText() {
  if (toggle_button_) {
    gtk_button_set_label(
        GTK_BUTTON(toggle_button_.get()),
        (g_current_anchor == WindowAnchor::kBottom) ? "▲" : "▼");
    int string_id = (g_current_anchor == WindowAnchor::kBottom)
                        ? IDS_MOVE_TO_TOP_BUTTON
                        : IDS_MOVE_TO_BOTTOM_BUTTON;
    std::string text = l10n_util::GetStringUTF8(string_id);
    gtk_widget_set_tooltip_text(toggle_button_.get(), text.c_str());

#if GTK_CHECK_VERSION(3, 90, 0)
    gtk_accessible_update_property(GTK_ACCESSIBLE(toggle_button_.get()),
                                   GTK_ACCESSIBLE_PROPERTY_LABEL, text.c_str(),
                                   -1);
#else
    AtkObject* atk_obj = gtk_widget_get_accessible(toggle_button_.get());
    if (atk_obj) {
      atk_object_set_name(atk_obj, text.c_str());
    }
#endif
  }
}

void DisconnectWindowGtk::SetDialogPosition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if !GTK_CHECK_VERSION(3, 90, 0)
  if (!disconnect_window_) {
    return;
  }

  GdkDisplay* display = gtk_widget_get_display(disconnect_window_.get());
  if (!display) {
    return;
  }

  GdkMonitor* monitor = nullptr;
  GdkWindow* gdk_window = gtk_widget_get_window(disconnect_window_.get());
  if (gdk_window) {
    monitor = gdk_display_get_monitor_at_window(display, gdk_window);
  }
  if (!monitor) {
    monitor = gdk_display_get_primary_monitor(display);
  }
  if (!monitor) {
    monitor = gdk_display_get_monitor(display, 0);
  }
  if (!monitor) {
    return;
  }

  GdkRectangle geometry;
  gdk_monitor_get_geometry(monitor, &geometry);

  int width = current_width_;
  int height = current_height_;
  if (width == 0 || height == 0) {
    GtkRequisition requisition;
    gtk_widget_get_preferred_size(disconnect_window_.get(), nullptr,
                                  &requisition);
    width = requisition.width;
    height = requisition.height;
  }

  int left = geometry.x + std::max(0, (geometry.width - width) / 2);
  int top = (g_current_anchor == WindowAnchor::kTop)
                ? (geometry.y + kTopMargin)
                : (geometry.y + geometry.height - height - kBottomMargin);

  gtk_window_move(GTK_WINDOW(disconnect_window_.get()), left, top);
#else
  NOTIMPLEMENTED_LOG_ONCE()
      << "Window positioning is not implemented for GTK4/Wayland.";
#endif
}

#if !GTK_CHECK_VERSION(3, 90, 0)
void DisconnectWindowGtk::OnMonitorsChanged(GdkScreen* screen) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetDialogPosition();
}
#endif

gboolean DisconnectWindowGtk::OnDelete(GtkWidget* window, GdkEvent* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (client_session_control_.get()) {
    client_session_control_->DisconnectSession(
        ErrorCode::OK, "Disconnect window deleted.", FROM_HERE);
  }
  return TRUE;
}

gboolean DisconnectWindowGtk::OnConfigure(GtkWidget* widget,
                                          GdkEventConfigure* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Only generate bitmaps if the size has actually changed.
  if (event->width == current_width_ && event->height == current_height_) {
    return FALSE;
  }

  current_width_ = event->width;
  current_height_ = event->height;

  SetDialogPosition();

  // gdk_window_set_back_pixmap() is not supported in GDK3, and
  // background drawing is handled in OnDraw().
  return FALSE;
}

gboolean DisconnectWindowGtk::OnDraw(GtkWidget* widget, cairo_t* cr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DrawBackground(cr, current_width_, current_height_);
  return FALSE;
}

}  // namespace

// static
std::unique_ptr<HostWindow> HostWindow::CreateDisconnectWindow() {
  return std::make_unique<DisconnectWindowGtk>();
}

}  // namespace remoting
